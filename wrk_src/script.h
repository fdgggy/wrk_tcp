#ifndef SCRIPT_H
#define SCRIPT_H
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <stdlib.h>
#include <string.h>
#include "wrk.h"

lua_State *create_luastate();
void script_start(lua_State *, uint16_t, uint64_t);
void script_connected(lua_State *, int);
void script_http_writeable(lua_State *L, int fd, char **buf, int *sz);
void script_writeable(lua_State *L, int fd, char **buf, int *sz);
int script_readable(lua_State *L, int fd, unsigned char *buffer, int sz, int *);
uint64_t script_delay(lua_State *L, int fd);
void script_heartbeat(lua_State *L, int fd);
void script_response(lua_State *L, int fd, int status, buffer *headers, buffer *body);
void buffer_append(buffer *b, const char *data, size_t len);
void buffer_reset(buffer *b);

#endif /* SCRIPT_H */
