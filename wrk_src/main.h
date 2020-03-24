#ifndef MAIN_H
#define MAIN_H
// #include <signal.h>
// #include <unistd.h>
// #include <sys/time.h>

static void *thread_main(void *);
static int connect_socket(thread *, connection *);

static void socket_connected(aeEventLoop *, int, void *, int);
static void socket_writeable(aeEventLoop *, int, void *, int);
static void socket_readable(aeEventLoop *, int, void *, int);
static int response_complete(http_parser *parser);
static int response_body(http_parser *parser, const char *at, size_t len);
static void print_stats_header();
static void print_stats(char *name, stats *stats);
#endif /* MAIN_H */
