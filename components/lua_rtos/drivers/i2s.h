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

#define I2S_TRANSACTION_INITIALIZER -1

// Internal driver structure
typedef struct i2s {
  uint8_t mode;
  uint8_t setup;
  struct mtx mtx;
} i2s_t;

// Resources used by I2S
typedef struct {
  uint8_t ws;
  uint8_t bck;
  uint8_t din;
  uint8_t dout;
} i2s_resources_t;

#if 0
#define I2C_SLAVE	0
#define I2C_MASTER	1
#endif

// I2S errors
#define I2S_ERR_IS_NOT_SETUP             (DRIVER_EXCEPTION_BASE(I2S_DRIVER_ID) |  0)
#define I2S_ERR_INVALID_UNIT             (DRIVER_EXCEPTION_BASE(I2S_DRIVER_ID) |  1)
#define I2S_ERR_NOT_ENOUGH_MEMORY		 (DRIVER_EXCEPTION_BASE(I2S_DRIVER_ID) |  2)
// #define I2C_ERR_CANT_INIT                (DRIVER_EXCEPTION_BASE(I2C_DRIVER_ID) |  0)
// #define I2C_ERR_INVALID_OPERATION		 (DRIVER_EXCEPTION_BASE(I2C_DRIVER_ID) |  3)
// #define I2C_ERR_INVALID_TRANSACTION		 (DRIVER_EXCEPTION_BASE(I2C_DRIVER_ID) |  5)
// #define I2C_ERR_NOT_ACK					 (DRIVER_EXCEPTION_BASE(I2C_DRIVER_ID) |  6)
// #define I2C_ERR_TIMEOUT					 (DRIVER_EXCEPTION_BASE(I2C_DRIVER_ID) |  7)

void i2s_init();

driver_error_t *i2s_lua_setup(int unit, int mode);
driver_error_t *i2s_lua_start(int unit);
driver_error_t *i2s_lua_stop(int unit);
#if 0
driver_error_t *i2s_write_address(int unit, int *transaction, char address, int read);
driver_error_t *i2s_write(int unit, int *transaction, char *data, int len);
driver_error_t *i2s_read(int unit, int *transaction, char *data, int len);
driver_error_t *i2s_flush(int unit, int *transaction, int new_transaction);
#endif

#endif /* I2S_H */
