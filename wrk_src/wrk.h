#ifndef WRK_H
#define WRK_H

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <sys/time.h>

#include <pthread.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/tcp.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/errno.h>
#include <unistd.h>
#include <signal.h>
#include "config.h"
#include "zmalloc.h"
#include "ae.h"
#include "stats.h"
#include "http_parser.h"

#define RECVBUF  8192 

#define MAX_THREAD_RATE_S   10000000
#define HEARTBEAT_INTERVAL_MS  20
#define RECORD_INTERVAL_MS  100

typedef struct {
    uint64_t  connections;
    uint64_t  duration;
    uint64_t  threads;
    uint64_t  timeout;
    bool delay;
    bool latency;
    char *host;
    int  port;
    char *url;
} config;

typedef struct {
    char  *buffer;
    size_t length;
    char  *cursor;
} buffer;

typedef struct {
    pthread_t thread;
    aeEventLoop *loop;
    uint16_t identify;
    uint64_t connections;
    uint64_t reqcount;
    uint64_t complete;
    uint64_t requests;
    uint64_t bytes;
    uint64_t start;
    lua_State *L;
    errors errors;
    struct connection *cs;
} thread;

typedef struct connection{
    thread *thread;
    http_parser parser;
    int fd;
    uint64_t start;
    char *request;
    int length;
    int written;
    bool http;
    char *host;
    int  port;
    buffer headers;
    buffer body;
    unsigned char buf[RECVBUF];
} connection;




#endif /* WRK_H */
