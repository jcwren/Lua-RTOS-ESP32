/*
 * Lua RTOS, I2S Lua module
 *
 * Copyright (C) 2015 - 2017
 * IBEROXARXA SERVICIOS INTEGRALES, S.L. & CSS IBÉRICA, S.L.
 *
 * Author: Jaume Olivé (jolive@iberoxarxa.com / jolive@whitecatboard.org)
 *
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software
 * and its documentation for any purpose and without fee is hereby
 * granted, provided that the above copyright notice appear in all
 * copies and that both that the copyright notice and this
 * permission notice and warranty disclaimer appear in supporting
 * documentation, and that the name of the author not be used in
 * advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.
 *
 * The author disclaim all warranties with regard to this
 * software, including all implied warranties of merchantability
 * and fitness.  In no event shall the author be liable for any
 * special, indirect or consequential damages or any damages
 * whatsoever resulting from loss of use, data or profits, whether
 * in an action of contract, negligence or other tortious action,
 * arising out of or in connection with the use or performance of
 * this software.
 */

#include "luartos.h"

#if CONFIG_LUA_RTOS_LUA_USE_I2S

#include "lua.h"
#include "error.h"
#include "lauxlib.h"
#include "i2s.h"
#include "modules.h"
#include "error.h"

#include <drivers/i2s.h>
#include <drivers/cpu.h>

extern LUA_REG_TYPE i2s_error_map[];
extern driver_message_t i2s_errors[];

typedef struct {
	int unit;
} i2s_userdata_t;

static int li2s_setup( lua_State* L ) {
	driver_error_t *error;

    int unit = luaL_checkinteger(L, 1);
    int mode = luaL_checkinteger(L, 2);

    if ((error = i2s_lua_setup(unit, mode))) {
    	return luaL_driver_error(L, error);
    }

    i2s_userdata_t *user_data = (i2s_userdata_t *)lua_newuserdata(L, sizeof(i2s_userdata_t));
    if (!user_data) {
       	return luaL_exception(L, I2S_ERR_NOT_ENOUGH_MEMORY);
    }

    user_data->unit = unit;

    luaL_getmetatable(L, "i2s.ins");
    lua_setmetatable(L, -2);

    return 1;
}

static int li2s_start( lua_State* L ) {
	driver_error_t *error;

    int unit = luaL_checkinteger(L, 1);

    if ((error = i2s_lua_start(unit))) {
    	return luaL_driver_error(L, error);
    }

     return 0;
}

static int li2s_stop( lua_State* L ) {
	driver_error_t *error;

    int unit = luaL_checkinteger(L, 1);

    if ((error = i2s_lua_stop(unit))) {
    	return luaL_driver_error(L, error);
    }

    return 0;
}

// Destructor
static int li2s_ins_gc (lua_State *L) {
    i2s_userdata_t *udata = NULL;
    udata = (i2s_userdata_t *)luaL_checkudata(L, 1, "i2s.ins");
	if (udata) {
	}

	return 0;
}

static const LUA_REG_TYPE li2s_map[] = {
    { LSTRKEY( "setup" ), LFUNCVAL( li2s_setup    ) },
	{ LSTRKEY( "error" ), LROVAL  ( i2s_error_map ) },
	I2S_I2S0
	I2S_I2S1
    { LNILKEY, LNILVAL }
};

static const LUA_REG_TYPE li2s_ins_map[] = {
	{ LSTRKEY( "start"       ), LFUNCVAL( li2s_start   ) },
    { LSTRKEY( "stop"        ), LFUNCVAL( li2s_stop    ) },
    { LSTRKEY( "__metatable" ), LROVAL  ( li2s_ins_map ) },
	{ LSTRKEY( "__index"     ), LROVAL  ( li2s_ins_map ) },
	{ LSTRKEY( "__gc"        ), LROVAL  ( li2s_ins_gc  ) },
    { LNILKEY, LNILVAL }
};

LUALIB_API int luaopen_i2s( lua_State *L ) {
    luaL_newmetarotable(L,"i2s.ins", (void *)li2s_ins_map);
    return 0;
}

MODULE_REGISTER_MAPPED(I2S, i2s, li2s_map, luaopen_i2s);

#endif
