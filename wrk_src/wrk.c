
#include "wrk.h"
#include "script.h"
#include "net.h"
#include "main.h"
#include <netdb.h>

static config cfg;
static volatile sig_atomic_t stop = 0;

static struct {
    stats *latency;
    stats *requests;
} statistics;

static struct sock sock = {
    .connect  = sock_connect,
    .close    = sock_close,
    .read     = sock_read,
    .write    = sock_write,
    .readable = sock_readable
};

static struct http_parser_settings parser_settings = {
    .on_message_complete = response_complete,
    .on_body             = response_body,
};

static void handler(int sig) {
    stop = 1;
}

bool init_config(config *c) {
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    luaL_loadfile(L, "config.lua");
    lua_pcall(L, 0, 1, 0); //-1 table

    lua_pushnil(L);  //-1 nil -2 table
    while (lua_next(L, -2) != 0) { // pop -1 
    	int keyt = lua_type(L, -2);
		if (keyt != LUA_TSTRING) {
            return false;
		}
        const char *key = lua_tostring(L, -2);	//-1 value -2 key -3 table
        if (strcmp(key, "connections") == 0) {
            c->connections = lua_tointeger(L, -1);
        }else if (strcmp(key, "threads") == 0) {
            c->threads = lua_tointeger(L, -1);
        }else if (strcmp(key, "host") == 0) {
            const char *value = lua_tostring(L, -1);
            int len = strlen(value)+1;
            c->host = zcalloc(len);
            memcpy(c->host, value, len);
        }else if (strcmp(key, "duration") == 0) {
            c->duration = lua_tointeger(L, -1);
        }else if (strcmp(key, "port") == 0) {
            c->port = lua_tointeger(L, -1);
        }else if (strcmp(key, "timeout") == 0) {
            c->timeout = lua_tointeger(L, -1);
        }else if (strcmp(key, "url") == 0) {
            const char *value = lua_tostring(L, -1);
            int len = strlen(value)+1;
            c->url = zcalloc(len);
            memcpy(c->url, value, len);
        }

        lua_pop(L, 1); //pop value => -1 key -2 table
    }
    lua_close(L);

    return true;
}

static uint64_t time_us() {
    struct timeval t;
    gettimeofday(&t, NULL);
    return (t.tv_sec * 1000000) + t.tv_usec;
}

int record_rate(aeEventLoop *loop, long long id, void *data) {
    // thread *thread = data;
    if (stop) aeStop(loop);

    return RECORD_INTERVAL_MS;
}

static int connect_socket(thread *thread, connection *c) {
    struct aeEventLoop *loop = thread->loop;
    int fd, flags;

    fd = socket(AF_INET,SOCK_STREAM,0);
    if (fd == -1) {
        printf("socket failed, errno:%d, errmsg:%s\n", errno, strerror(errno));
        goto error;
    }

    flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

	struct sockaddr_in my_addr;
	my_addr.sin_addr.s_addr=inet_addr(c->host);
	my_addr.sin_family=AF_INET;
	my_addr.sin_port=htons(c->port);

    if (connect(fd, (struct sockaddr *)&my_addr,sizeof(struct sockaddr_in)) == -1) {
        if (errno != EINPROGRESS) { //正在连接中
            printf("connect failed, errno:%d, errmsg:%s\n", errno, strerror(errno));
            goto error;
        }
    }

    flags = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flags, sizeof(flags)); //禁用Nagle算法

    flags = AE_READABLE | AE_WRITABLE;
    if (aeCreateFileEvent(loop, fd, flags, socket_connected, c) == AE_OK) {
        if (c->http) {
            c->parser.data = c;
        }
        c->fd = fd;
        return fd;
    }

  error:
    printf("connect error\n");
    thread->errors.connect++;
    close(fd);
    return -1;
}

static void socket_connected(aeEventLoop *loop, int fd, void *data, int mask) {
    connection *c = data;

    switch (sock.connect(c, cfg.host)) {
        case OK:    break;
        case ERROR: goto error;
        case RETRY: return;
    }
    if (c->http) {
        http_parser_init(&c->parser, HTTP_RESPONSE);
    }

    script_connected(c->thread->L, fd);
    aeCreateFileEvent(c->thread->loop, fd, AE_READABLE, socket_readable, c);
    aeCreateFileEvent(c->thread->loop, fd, AE_WRITABLE, socket_writeable, c);

    stats_connect(statistics.latency);
    // printf("statistics.latency:%"PRIu64"\n", statistics.latency->connects);

    return;

  error:
    c->thread->errors.connect++;
}

static void socket_writeable(aeEventLoop *loop, int fd, void *data, int mask) {
    connection *c = data;
    thread *thread = c->thread;

    if (c->written == 0) {
        if (c->http) {
            script_http_writeable(thread->L, fd, &c->request, &c->length);
        }else {
            script_writeable(thread->L, fd, &c->request, &c->length);
        }
    }
    if (c->length == 0) {
        aeDeleteFileEvent(loop, fd, AE_WRITABLE);
    }else {
        c->start   = time_us();

        char  *buf = c->request + c->written; //非阻塞套接字有可能缓冲区不够，会多次写
        int len = c->length  - c->written;
        size_t n;

        switch (sock.write(c, buf, len, &n)) {  //应用程序写入的数据大于套接字缓冲区大小，这将会发生拆包。
            case OK:    break;
            case ERROR: goto error;  //内核缓冲区没有空间
            case RETRY: return;
        }
        c->written += n;
        if (c->written == c->length) {
            thread->reqcount++;
            c->length = c->written = 0;
            free(c->request);
            c->request = NULL;
            aeDeleteFileEvent(loop, fd, AE_WRITABLE);
        }
    }

    return;

  error:
    printf("write failed, errno:%d, errmsg:%s\n", errno, strerror(errno));
    thread->errors.write++;
}

static int response_body(http_parser *parser, const char *at, size_t len) {
    connection *c = parser->data;
    buffer_append(&c->body, at, len);
    return 0;
}

static int response_complete(http_parser *parser) {
    connection *c = parser->data;
    thread *thread = c->thread;

    int status = parser->status_code;
    if (status > 399) {
        thread->errors.status++;
    }
    uint64_t now = time_us();
    thread->complete++;
    if (!stats_record(statistics.latency, now - c->start)) {
        thread->errors.timeout++;
    }
    script_response(thread->L, c->fd, status, &c->headers, &c->body);
    http_parser_init(parser, HTTP_RESPONSE);

    return 0;
}

static void socket_readable(aeEventLoop *loop, int fd, void *data, int mask) {
    connection *c = data;
    thread *thread = c->thread;

    size_t n;
    do {
        switch (sock.read(c, &n)) {
            case OK:    break;
            case ERROR: goto error;
            case RETRY: return;
        }
        if(n == 0) { //end of file
            break;
        }

        if (c->http) {
            if (http_parser_execute(&c->parser, &parser_settings, (char*)c->buf, n) != n) goto error;
            if (n == 0 && !http_body_is_final(&c->parser)) goto error;
        }else {
            unsigned char * ptr = malloc(sizeof(unsigned char)*n);
            memmove(ptr, c->buf, n);

            int readover = 0;
            int result = script_readable(thread->L, fd, ptr, n, &readover);

            if (readover == 0) {
                uint64_t now = time_us();
                if (!stats_record(statistics.latency, now - c->start)) {
                    thread->errors.timeout++;
                }
                
                thread->complete += result;
                aeCreateFileEvent(thread->loop, c->fd, AE_WRITABLE, socket_writeable, c);
            }else if (readover == -1){
                thread->complete += result;
            }else if (readover == -2){
                // printf("ccccc\n");
            }
        }

        c->thread->bytes += n;
    } while (n == RECVBUF && sock.readable(c) > 0);

    return;

  error:
    printf("read failed, errno:%d, errmsg:%s\n", errno, strerror(errno));
    c->thread->errors.read++;
}

static char *copy_url_part(char *url, struct http_parser_url *parts, enum http_parser_url_fields field) {
    char *part = NULL;

    if (parts->field_set & (1 << field)) {
        uint16_t off = parts->field_data[field].off;
        uint16_t len = parts->field_data[field].len;
        part = zcalloc(len + 1 * sizeof(char));
        memcpy(part, &url[off], len);
    }

    return part;
}

void http_addr(char ** host, int *port) {
    struct addrinfo *addrs;
    struct addrinfo hints = {
        .ai_family   = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM
    };
    struct http_parser_url parts = {};
    http_parser_parse_url(cfg.url, strlen(cfg.url), 0, &parts);
    char * domain  = copy_url_part(cfg.url, &parts, UF_HOST);
    char * service = copy_url_part(cfg.url, &parts, UF_PORT);

    getaddrinfo(domain, service, &hints, &addrs);
    getnameinfo(addrs->ai_addr, addrs->ai_addrlen, domain, NI_MAXHOST, NULL, NI_MAXSERV, NI_NUMERICHOST|NI_NUMERICSERV);
    *host = domain;
    *port = atoi(service);

    freeaddrinfo(addrs);
}

void *thread_main(void *arg) {
    thread *thread = arg;

    script_start(thread->L, thread->identify, thread->connections);

    thread->cs = zcalloc(thread->connections * sizeof(connection));
    connection *c = thread->cs;
    for (int i = 0; i < thread->connections; i++, c++) {
        c->thread = thread;
        if (i == -1) {
            c->http = true;
            http_addr(&(c->host), &c->port);
        }else {
            c->http = false;

            c->host = cfg.host;
            c->port = cfg.port; 
        }
        c->request = NULL;
        c->length = c->written = 0;
        connect_socket(thread, c);
    }
    aeEventLoop *loop = thread->loop;
    aeCreateTimeEvent(loop, RECORD_INTERVAL_MS, record_rate, thread, NULL);
    
    aeMain(loop);
    aeDeleteEventLoop(loop);
    zfree(thread->cs);

    lua_close(thread->L);

    return NULL;
}

int main() {
    if (init_config(&cfg) == false) {
        printf("invalid config param, config key muse be string, check please.\n");
        return -1;
    }
    if (!cfg.connections || cfg.connections < cfg.threads) {
        printf("number of connections must be >= threads\n");
        return -1;
    }

    statistics.latency  = stats_alloc(cfg.timeout * 1000);
    thread *threads     = zcalloc(cfg.threads * sizeof(thread));
    for (int i = 0; i < cfg.threads; i++) {
        thread *t      = &threads[i];
        t->identify    = i + 1;
        t->loop        = aeCreateEventLoop(10 + cfg.connections * 3);
        t->connections = cfg.connections / cfg.threads;

        t->L = create_luastate();

        if (pthread_create(&t->thread, NULL, &thread_main, t)) {
            printf("create thread failed!");
            exit(2);
        }
    }

    struct sigaction sa = {
        .sa_handler = handler,
        .sa_flags   = 0,
    };

    sigfillset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);

    printf("Running %"PRIu64"s test skynet\n", cfg.duration);
    printf("  %"PRIu64" threads and %"PRIu64" connections\n", cfg.threads, cfg.connections);

    uint64_t start    = time_us();
    uint64_t reqcount = 0;
    uint64_t complete = 0;
    uint64_t bytes    = 0;
    errors errors     = { 0 };

    sleep(cfg.duration);
    stop = 1;

    for (int i = 0; i < cfg.threads; i++) {
        thread *t = &threads[i];
        pthread_join(t->thread, NULL);
        reqcount += t->reqcount;
        complete += t->complete;
        bytes    += t->bytes;

        errors.connect += t->errors.connect;
        errors.read    += t->errors.read;
        errors.write   += t->errors.write;
        errors.timeout += t->errors.timeout;
    }
    uint64_t runtime_us = time_us() - start;
    long double runtime_s   = runtime_us / 1000000.0;
    long double req_per_s   = reqcount   / runtime_s;
    long double res_per_s   = complete   / runtime_s;
    long double bytes_per_s = bytes      / runtime_s;

    printf("Running %"PRIu64"s test skynet\n", cfg.duration);
    printf("  %"PRIu64" threads and actually %"PRIu64" connections\n", cfg.threads, statistics.latency->connects);
    print_stats_header();
    print_stats("Latency", statistics.latency);

    printf("  %"PRIu64" requests %"PRIu64" responses in %.2f ms, %.2f kb read\n", reqcount, complete, runtime_us/1000.0, bytes/1024.0);
    if (errors.connect || errors.read || errors.write || errors.timeout) {
        printf("  Socket errors: connect %d, read %d, write %d, timeout %d\n",
               errors.connect, errors.read, errors.write, errors.timeout);
    }

    printf("Requests/sec: %9.2Lf\n", req_per_s);
    printf("Responses/sec: %9.2Lf\n", res_per_s);
    printf("Transfer/sec: %10.2Lf kb\n", bytes_per_s/1024.0);

    return 0;
}

static void print_stats_header() {
    printf("  Thread Stats%10s%10s%10s\n", "Avg", "Min", "Max");
}

static void print_stats(char *name, stats *stats) {
    uint64_t min = stats->min;
    uint64_t max = stats->max;
    long double mean  = stats_mean(stats);

    printf("    %-10s", name);
    printf("    %.2Lf ms", mean/1000);
    printf("    %.2f ms", min/1000.0);
    printf("    %.2f ms\n", max/1000.0);
}