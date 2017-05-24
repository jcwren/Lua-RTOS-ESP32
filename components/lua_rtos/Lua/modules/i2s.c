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

#include <string.h>

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

static int li2s_sanity (lua_State *L, int unit) {
  if (!i2s_lua_exists (unit))
    return luaL_error(L, "I2S%d does not exist", unit);
  if (!i2s_lua_is_setup (unit))
    return luaL_error(L, "I2S%d is not setup", unit);

  return 0;
}

static int li2s_setup( lua_State* L ) {
  driver_error_t *error;
  int unit;
  int queue_size;
  i2s_config_t config;
  i2s_pin_config_t pin;

  memset (&config, 0, sizeof (config));
  memset (&pin, 0, sizeof (pin));

  unit = luaL_checkinteger(L, 1);

  config.mode                 = luaL_checkinteger(L, 2);
  config.sample_rate          = luaL_checkinteger(L, 3);
  config.channel_format       = luaL_checkinteger(L, 4);
  config.communication_format = luaL_checkinteger(L, 5);
  config.dma_buf_count        = luaL_checkinteger(L, 6);
  config.dma_buf_len          = luaL_checkinteger(L, 7);
  config.intr_alloc_flags     = luaL_checkinteger(L, 8);

  pin.bck_io_num   = luaL_checkinteger(L, 9);
  pin.ws_io_num    = luaL_checkinteger(L, 10);
  pin.data_out_num = luaL_checkinteger(L, 11);
  pin.data_in_num  = luaL_checkinteger(L, 12);

  queue_size = luaL_checkinteger(L, 13);

  if ((error = i2s_lua_setup(unit, &config, &pin, queue_size)))
    return luaL_driver_error(L, error);

  i2s_userdata_t *user_data = (i2s_userdata_t *)lua_newuserdata(L, sizeof(i2s_userdata_t));
  if (!user_data)
    return luaL_exception(L, I2S_ERR_NOT_ENOUGH_MEMORY);

  user_data->unit = unit;

  luaL_getmetatable(L, "i2s.ins");
  lua_setmetatable(L, -2);

  return 1;
}

static int li2s_start( lua_State* L ) {
  int errval;
  driver_error_t *error;

  int unit = luaL_checkinteger(L, 1);

  if ((errval = li2s_sanity(L, unit)))
    return errval;

  if ((error = i2s_lua_start(unit)))
    return luaL_driver_error(L, error);

  return 0;
}

static int li2s_stop( lua_State* L ) {
  int errval;
  driver_error_t *error;

  int unit = luaL_checkinteger(L, 1);

  if ((errval = li2s_sanity(L, unit)))
    return errval;

  if ((error = i2s_lua_stop(unit)))
    return luaL_driver_error(L, error);

  return 0;
}

static int li2s_write( lua_State* L ) {
  int errval;
  driver_error_t *error;

  int unit = luaL_checkinteger(L, 1);
  int size = luaL_checkinteger(L, 2);
  const char *src = luaL_checkstring(L, 3);
  int ticks_to_wait = luaL_optinteger(L, 4, portMAX_DELAY);

  if ((errval = li2s_sanity(L, unit)))
    return errval;

  if ((error = i2s_lua_write(unit, (void *) src, size, ticks_to_wait)))
    return luaL_driver_error(L, error);

  return 0;
}

static int li2s_read( lua_State* L ) {
  int errval;
  driver_error_t *error;

  int unit = luaL_checkinteger(L, 1);
  int size = luaL_checkinteger(L, 2);
  int ticks_to_wait = luaL_optinteger(L, 3, portMAX_DELAY);
  char *dest = NULL;

  if ((errval = li2s_sanity(L, unit)))
    return errval;

  if ((error = i2s_lua_read(unit, (void *) dest, size, ticks_to_wait)))
    return luaL_driver_error(L, error);

  lua_pushlstring(L, (const char *) dest, (size_t) size);

  return 0;
}

static int li2s_push( lua_State* L ) {
  int errval;
  driver_error_t *error;

  int unit = luaL_checkinteger(L, 1);
  int sample = luaL_checkinteger(L, 2);
  int ticks_to_wait = luaL_optinteger(L, 3, portMAX_DELAY);

  if ((errval = li2s_sanity(L, unit)))
    return errval;

  if ((error = i2s_lua_push(unit, (void *) &sample, ticks_to_wait)))
    return luaL_driver_error(L, error);

  return 0;
}

static int li2s_pop( lua_State* L ) {
  int errval;
  driver_error_t *error;

  int unit = luaL_checkinteger(L, 1);
  int sample = luaL_checkinteger(L, 2);
  int ticks_to_wait = luaL_optinteger(L, 3, portMAX_DELAY);

  if ((errval = li2s_sanity(L, unit)))
    return errval;

  if ((error = i2s_lua_push(unit, (void *) sample, ticks_to_wait)))
    return luaL_driver_error(L, error);

  return 0;
}

static int li2s_zerobuf( lua_State* L ) {
  int errval;
  driver_error_t *error;

  int unit = luaL_checkinteger(L, 1);

  if ((errval = li2s_sanity(L, unit)))
    return errval;

  if ((error = i2s_lua_zerobuf(unit)))
    return luaL_driver_error(L, error);

  return 0;
}

static int li2s_setclk( lua_State* L ) {
  int errval;
  driver_error_t *error;

  int unit = luaL_checkinteger(L, 1);
  int rate = luaL_checkinteger(L, 2);

  if ((errval = li2s_sanity(L, unit)))
    return errval;

  if ((error = i2s_lua_setclk(unit, rate)))
    return luaL_driver_error(L, error);

  return 0;
}

static int li2s_setrate( lua_State* L ) {
  int errval;
  driver_error_t *error;

  int unit = luaL_checkinteger(L, 1);
  int rate = luaL_checkinteger(L, 2);
  int bits = luaL_checkinteger(L, 3);
  int channel = luaL_checkinteger(L, 4);

  if ((errval = li2s_sanity(L, unit)))
    return errval;

  if ((error = i2s_lua_setrate(unit, rate, bits, channel)))
    return luaL_driver_error(L, error);

  return 0;
}

static int li2s_dacmode( lua_State* L ) {
  driver_error_t *error;

  int dacmode = luaL_checkinteger(L, 1);

  if ((error = i2s_lua_dacmode(dacmode))) {
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

static const LUA_REG_TYPE li2s_bps_map[] = {
  { LSTRKEY( "8B"  ), LINTVAL( I2S_BITS_PER_SAMPLE_8BIT  ) },
  { LSTRKEY( "16B" ), LINTVAL( I2S_BITS_PER_SAMPLE_16BIT ) },
  { LSTRKEY( "24B" ), LINTVAL( I2S_BITS_PER_SAMPLE_24BIT ) },
  { LSTRKEY( "32B" ), LINTVAL( I2S_BITS_PER_SAMPLE_32BIT ) },
  { LNILKEY, LNILVAL }
};

static const LUA_REG_TYPE li2s_channel_map[] = {
  { LSTRKEY( "MONO" ),   LINTVAL( I2S_CHANNEL_MONO   ) },
  { LSTRKEY( "STEREO" ), LINTVAL( I2S_CHANNEL_STEREO ) },
  { LNILKEY, LNILVAL }
};

static const LUA_REG_TYPE li2s_commfmt_map[] = { // bit mapped
  { LSTRKEY( "I2S"      ), LINTVAL( I2S_COMM_FORMAT_I2S       ) },
  { LSTRKEY( "I2SMSB"   ), LINTVAL( I2S_COMM_FORMAT_I2S_MSB   ) },
  { LSTRKEY( "I2SLSB"   ), LINTVAL( I2S_COMM_FORMAT_I2S_LSB   ) },
  { LSTRKEY( "PCM"      ), LINTVAL( I2S_COMM_FORMAT_PCM       ) },
  { LSTRKEY( "PCMSHORt" ), LINTVAL( I2S_COMM_FORMAT_PCM_SHORT ) },
  { LSTRKEY( "PCMLONG"  ), LINTVAL( I2S_COMM_FORMAT_PCM_LONG  ) },
  { LNILKEY, LNILVAL }
};

static const LUA_REG_TYPE li2s_chanfmt_map[] = {
  { LSTRKEY( "RL" ), LINTVAL( I2S_CHANNEL_FMT_RIGHT_LEFT ) },
  { LSTRKEY( "AR" ), LINTVAL( I2S_CHANNEL_FMT_ALL_RIGHT  ) },
  { LSTRKEY( "AL" ), LINTVAL( I2S_CHANNEL_FMT_ALL_LEFT   ) },
  { LSTRKEY( "OR" ), LINTVAL( I2S_CHANNEL_FMT_ONLY_RIGHT ) },
  { LSTRKEY( "OL" ), LINTVAL( I2S_CHANNEL_FMT_ONLY_LEFT  ) },
  { LNILKEY, LNILVAL }
};

static const LUA_REG_TYPE li2s_pdmsrr_map[] = {
  { LSTRKEY( "64"  ), LINTVAL( PDM_SAMPLE_RATE_RATIO_64  ) },
  { LSTRKEY( "128" ), LINTVAL( PDM_SAMPLE_RATE_RATIO_128 ) },
  { LNILKEY, LNILVAL }
};

static const LUA_REG_TYPE li2s_pdmconv_map[] = {
  { LSTRKEY( "ENABLE"  ), LINTVAL( PDM_PCM_CONV_ENABLE  ) },
  { LSTRKEY( "DISABLE" ), LINTVAL( PDM_PCM_CONV_DISABLE ) },
  { LNILKEY, LNILVAL }
};

static const LUA_REG_TYPE li2s_mode_map[] = { // bit mapped
  { LSTRKEY( "MASTER" ), LINTVAL( I2S_MODE_MASTER       ) },
  { LSTRKEY( "SLAVE"  ), LINTVAL( I2S_MODE_SLAVE        ) },
  { LSTRKEY( "TX"     ), LINTVAL( I2S_MODE_TX           ) },
  { LSTRKEY( "RX"     ), LINTVAL( I2S_MODE_RX           ) },
  { LSTRKEY( "DAC"    ), LINTVAL( I2S_MODE_DAC_BUILT_IN ) },
  //  { LSTRKEY( "ADC"    ), LINTVAL( I2S_MODE_ADC_BUILT_IN ) },
  { LSTRKEY( "PDM"    ), LINTVAL( I2S_MODE_PDM          ) },
  { LNILKEY, LNILVAL }
};

static const LUA_REG_TYPE li2s_dac_map[] = { // bit mapped
  { LSTRKEY( "DISABLE" ), LINTVAL( I2S_DAC_CHANNEL_DISABLE  ) },
  { LSTRKEY( "RIGHT"   ), LINTVAL( I2S_DAC_CHANNEL_RIGHT_EN ) },
  { LSTRKEY( "LEFT"    ), LINTVAL( I2S_DAC_CHANNEL_LEFT_EN  ) },
  { LSTRKEY( "BOTH"    ), LINTVAL( I2S_DAC_CHANNEL_BOTH_EN  ) },
  { LSTRKEY( "MAX"     ), LINTVAL( I2S_DAC_CHANNEL_MAX      ) },
  { LNILKEY, LNILVAL }
};

static const LUA_REG_TYPE li2s_map[] = {
  { LSTRKEY( "setup" ),   LFUNCVAL( li2s_setup       ) },
  { LSTRKEY( "BPS" ),     LROVAL  ( li2s_bps_map     ) },
  { LSTRKEY( "CHANNEL" ), LROVAL  ( li2s_channel_map ) },
  { LSTRKEY( "COMMFMT" ), LROVAL  ( li2s_commfmt_map ) },
  { LSTRKEY( "CHANFMT" ), LROVAL  ( li2s_chanfmt_map ) },
  { LSTRKEY( "PDMSRR" ),  LROVAL  ( li2s_pdmsrr_map  ) },
  { LSTRKEY( "PDMCONV" ), LROVAL  ( li2s_pdmconv_map ) },
  { LSTRKEY( "MODE" ),    LROVAL  ( li2s_mode_map    ) },
  { LSTRKEY( "DAC" ),     LROVAL  ( li2s_dac_map     ) },
  { LSTRKEY( "error" ),   LROVAL  ( i2s_error_map    ) },
  I2S_I2S0
  I2S_I2S1
  { LNILKEY, LNILVAL }
};

static const LUA_REG_TYPE li2s_ins_map[] = {
  { LSTRKEY( "start"       ), LFUNCVAL( li2s_start   ) },
  { LSTRKEY( "stop"        ), LFUNCVAL( li2s_stop    ) },
  { LSTRKEY( "write"       ), LFUNCVAL( li2s_write   ) },
  { LSTRKEY( "read"        ), LFUNCVAL( li2s_read    ) },
  { LSTRKEY( "push"        ), LFUNCVAL( li2s_push    ) },
  { LSTRKEY( "pop"         ), LFUNCVAL( li2s_pop     ) },
  { LSTRKEY( "zerobuf"     ), LFUNCVAL( li2s_zerobuf ) },
  { LSTRKEY( "setclk"      ), LFUNCVAL( li2s_setclk  ) },
  { LSTRKEY( "setrate"     ), LFUNCVAL( li2s_setrate ) },
  { LSTRKEY( "dacmode"     ), LFUNCVAL( li2s_dacmode ) },
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
