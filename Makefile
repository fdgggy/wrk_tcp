include platform.mk

LUA_CLIB_PATH ?= luaclib
WRK_BUILD_PATH ?= .

WRK_BUILD_PATH ?= .
CFLAGS = -std=gnu99 -g -O2 -Wall -I$(LUA_INC) $(MYCFLAGS)

LUA_STATICLIB := 3rd/lua/liblua.a
LUA_LIB ?= $(LUA_STATICLIB)
LUA_INC ?= 3rd/lua

LUA_CLIB = socketdriver sproto lpeg netpack cjson
$(LUA_STATICLIB) :
	cd 3rd/lua && $(MAKE) CC='$(CC) -std=gnu99' $(PLAT)

WRK_SRC = wrk.c ae.c zmalloc.c net.c script.c stats.c http_parser.c

all: \
  $(WRK_BUILD_PATH)/wrk \
  $(foreach v, $(LUA_CLIB), $(LUA_CLIB_PATH)/$(v).so) 

$(WRK_BUILD_PATH)/wrk : $(foreach v, $(WRK_SRC), wrk_src/$(v)) $(LUA_LIB) 
	$(CC) $(CFLAGS) -o $@ $^ -Iwrk_src $(LDFLAGS) $(EXPORT) $(WRK_LIBS) 


$(LUA_CLIB_PATH) :
	mkdir $(LUA_CLIB_PATH)

$(LUA_CLIB_PATH)/socketdriver.so : lualib_src/lua_socket.c  | $(LUA_CLIB_PATH)
	$(CC) $(CFLAGS) $(SHARED) $^ -o $@ -Iwrk_src

$(LUA_CLIB_PATH)/netpack.so : lualib_src/lua_netpack.c  | $(LUA_CLIB_PATH)
	$(CC) $(CFLAGS) $(SHARED) $^ -o $@

$(LUA_CLIB_PATH)/sproto.so : lualib_src/sproto/sproto.c lualib_src/sproto/lsproto.c | $(LUA_CLIB_PATH)
	$(CC) $(CFLAGS) $(SHARED) -Ilualib_src/sproto $^ -o $@ 

$(LUA_CLIB_PATH)/lpeg.so : 3rd/lpeg/lpcap.c 3rd/lpeg/lpcode.c 3rd/lpeg/lpprint.c 3rd/lpeg/lptree.c 3rd/lpeg/lpvm.c | $(LUA_CLIB_PATH)
	$(CC) $(CFLAGS) $(SHARED) -I3rd/lpeg $^ -o $@ 

# $(LUA_CLIB_PATH)/cjson.so : | $(LUA_CLIB_PATH)
# 	cd 3rd/lua-cjson-2.1.0 && $(MAKE) LUA_INCLUDE_DIR=../../$(LUA_INC) CC=$(CC) CJSON_LDFLAGS="$(SHARED)" && cd ../.. && cp 3rd/lua-cjson-2.1.0/cjson.so $@

$(LUA_CLIB_PATH)/cjson.so : 3rd/lua-cjson-2.1.0/lua_cjson.c 3rd/lua-cjson-2.1.0/strbuf.c 3rd/lua-cjson-2.1.0/fpconv.c | $(LUA_CLIB_PATH)
	$(CC) $(CFLAGS) $(SHARED) -fgnu89-inline  -pedantic -DNDEBUG -DMULTIPLE_THREADS -I3rd/lua-cjson-2.1.0 $^ -o $@ 

clean:
	rm -f $(WRK_BUILD_PATH)/wrk  $(LUA_CLIB_PATH)/*.so

cleanall: clean
	cd 3rd/lua && $(MAKE) clean
	rm -f $(LUA_STATICLIB)

