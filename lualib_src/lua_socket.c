
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

static int
lsend(lua_State *L) {
    lua_pushnumber(L, 1000);

	return 1;
}

int
luaopen_socketdriver(lua_State *L) {
	luaL_Reg l[] = {
		{ "send" , lsend },
		{ NULL, NULL },
	};

	luaL_newlib(L, l);
	return 1;
}