#ifndef _TEK_LUA_H
#define _TEK_LUA_H

#include <lua.h>
#include <lauxlib.h>

#if LUA_VERSION_NUM < 502
#define tek_lua_register(L, classname, classfuncs, nup) \
do { \
	const luaL_Reg *___cfuncs = classfuncs; \
	if (classname) { \
		static const luaL_Reg nullreg[] = { { NULL, NULL } }; \
		luaL_register(L, classname, nullreg); \
		lua_insert(L, -1 - (nup)); \
	} \
	for (; ___cfuncs->name; ___cfuncs++) { \
		int i; \
		for (i = 0; i < (nup); i++) \
			lua_pushvalue(L, -(nup)); \
		lua_pushcclosure(L, ___cfuncs->func, nup); \
		lua_setfield(L, -((nup) + 2), ___cfuncs->name); \
	} \
	lua_pop(L, nup); \
} while (0)
#else
#define tek_lua_register(L, classname, l, nup) \
do { \
	if (classname) { \
		lua_newtable(L); \
		lua_insert(L, -1 - (nup)); \
	} \
	luaL_setfuncs(L, l, nup); \
} while (0)
#endif

#if LUA_VERSION_NUM < 502
#define tek_lua_equal(L, i1, i2) lua_equal(L, i1, i2)
#define tek_lua_len(L, i) lua_objlen(L, i)
#else
#define tek_lua_equal(L, i1, i2) lua_compare(L, i1, i2, LUA_OPEQ)
#define tek_lua_len(L, i) lua_rawlen(L, i)
#endif

#endif
