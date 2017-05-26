/*
 * Lua RTOS, I2C driver
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

#ifndef I2S_H
#define I2S_H

#include "driver/i2s.h"

#include <stdint.h>

#include <sys/driver.h>
#include <sys/mutex.h>

#include <drivers/cpu.h>

typedef struct i2s {
  struct mtx mtx;
  uint8_t setup;

  i2s_config_t config;
  int num_channels;
  int bytes_per_sample;
  int bytes_per_pushpop;
  i2s_pin_config_t pin;
  int evtqueue_size;
  void *evtqueue;
} i2s_t;

// I2S errors
#define I2S_ERR_IS_NOT_SETUP             (DRIVER_EXCEPTION_BASE(I2S_DRIVER_ID) |  0)
#define I2S_ERR_INVALID_UNIT             (DRIVER_EXCEPTION_BASE(I2S_DRIVER_ID) |  1)
#define I2S_ERR_NOT_ENOUGH_MEMORY        (DRIVER_EXCEPTION_BASE(I2S_DRIVER_ID) |  2)
#define I2S_ERR_DRIVER_INSTALL           (DRIVER_EXCEPTION_BASE(I2S_DRIVER_ID) |  3)
#define I2S_ERR_DRIVER_SET_PIN           (DRIVER_EXCEPTION_BASE(I2S_DRIVER_ID) |  4)
#define I2S_ERR_DRIVER_ERROR             (DRIVER_EXCEPTION_BASE(I2S_DRIVER_ID) |  5)
#define I2S_ERR_SIZE_GT_LEN              (DRIVER_EXCEPTION_BASE(I2S_DRIVER_ID) |  6)
#define I2S_ERR_BAD_SAMPLE_LENGTH        (DRIVER_EXCEPTION_BASE(I2S_DRIVER_ID) |  7)

void i2s_init ();

int i2s_lua_exists (int unit);
int i2s_lua_is_setup (int unit);
int i2s_lua_get_pushpop_size (int unit);
driver_error_t *i2s_lua_setup (int unit, const i2s_config_t *config, i2s_pin_config_t *i2s_pins, int evtqueue_size);
driver_error_t *i2s_lua_start (int unit);
driver_error_t *i2s_lua_stop (int unit);
driver_error_t *i2s_lua_write (int unit, void *src, size_t size, TickType_t ticks_to_wait);
driver_error_t *i2s_lua_read (int unit, void *dest, size_t size, TickType_t ticks_to_wait);
driver_error_t *i2s_lua_push (int unit, void *sample, TickType_t ticks_to_wait);
driver_error_t *i2s_lua_pop (int unit, void *sample, TickType_t ticks_to_wait);
driver_error_t *i2s_lua_zerobuf (int unit);
driver_error_t *i2s_lua_setclk (int unit, uint32_t rate);
driver_error_t *i2s_lua_setrate (int unit, uint32_t rate, int bits, int channel);
driver_error_t *i2s_lua_dacmode (int dacmode);

#endif /* I2S_H */
