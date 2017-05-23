/*
 * Lua RTOS, I2S driver
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

#include "freertos/FreeRTOS.h"
#include "driver/i2s.h"
#include "driver/periph_ctrl.h"

#include <stdint.h>

#include <sys/list.h>
#include <sys/mutex.h>
#include <sys/driver.h>
#include <sys/syslog.h>

#include <drivers/gpio.h>
#include <drivers/cpu.h>
#include <drivers/i2s.h>

// Driver locks
driver_unit_lock_t i2s_locks[CPU_LAST_I2S + 1];

DRIVER_REGISTER_ERROR(I2S, i2s, NotSetup, "is not setup", I2S_ERR_IS_NOT_SETUP);
DRIVER_REGISTER_ERROR(I2S, i2s, InvalidUnit, "invalid unit", I2S_ERR_INVALID_UNIT);
DRIVER_REGISTER_ERROR(I2S, i2s, NotEnoughtMemory, "not enough memory", I2S_ERR_NOT_ENOUGH_MEMORY);
#if 0
// Driver message errors
DRIVER_REGISTER_ERROR(I2C, i2c, CannotSetup, "can't setup", I2C_ERR_CANT_INIT);
DRIVER_REGISTER_ERROR(I2C, i2c, InvalidOperation,"invalid operation", I2C_ERR_INVALID_OPERATION);
DRIVER_REGISTER_ERROR(I2C, i2c, InvalidTransaction, "invalid transaction", I2C_ERR_INVALID_TRANSACTION);
DRIVER_REGISTER_ERROR(I2C, i2c, AckNotReceived, "not ack received", I2C_ERR_NOT_ACK);
DRIVER_REGISTER_ERROR(I2C, i2c, Timeout, "timeout", I2C_ERR_TIMEOUT);
#endif

// i2s info needed by driver
static i2s_t i2s[CPU_LAST_I2S + 1] = {
	{0,0, MUTEX_INITIALIZER},
	{0,0, MUTEX_INITIALIZER},
};

/*
 * Helper functions
 */
static driver_error_t *i2s_lock_resources(int unit, i2s_resources_t *resources) {
	driver_unit_lock_error_t *lock_error = NULL;

	// Lock ws
	if ((lock_error = driver_lock(I2S_DRIVER, unit, GPIO_DRIVER, resources->ws))) {
    	return driver_lock_error(I2S_DRIVER, lock_error);
    }

	// Lock bck
	if ((lock_error = driver_lock(I2S_DRIVER, unit, GPIO_DRIVER, resources->bck))) {
    	return driver_lock_error(I2S_DRIVER, lock_error);
    }

	// Lock din
    if (resources->din != (uint8_t) -1) {
	    if ((lock_error = driver_lock(I2S_DRIVER, unit, GPIO_DRIVER, resources->din))) {
    	    return driver_lock_error(I2S_DRIVER, lock_error);
        }
    }

	// Lock dout
    if (resources->dout != (uint8_t) -1) {
	    if ((lock_error = driver_lock(I2S_DRIVER, unit, GPIO_DRIVER, resources->dout))) {
    	    return driver_lock_error(I2S_DRIVER, lock_error);
        }
    }

	return NULL;
}

static driver_error_t *i2s_check(int unit) {
    // Sanity checks
	if (!((1 << unit) & CPU_I2S_ALL)) {
		return driver_operation_error(I2S_DRIVER, I2S_ERR_INVALID_UNIT, NULL);
	}

	if (!i2s[unit].setup) {
		return driver_operation_error(I2S_DRIVER, I2S_ERR_IS_NOT_SETUP, NULL);
	}

	return NULL;
}

/*
 * Operation functions
 */

void i2s_lua_init() {
	int i;

    // Init mutexes
    for (i = 0; i < CPU_LAST_I2S; i++) {
        mtx_init (&i2s [i].mtx, NULL, NULL, 0);
    }
}

driver_error_t *i2s_lua_setup(int unit, int mode) {
	driver_error_t *error;
    uint8_t gpio_ws = 1;
    uint8_t gpio_bck = 2;
    uint8_t gpio_din = 3;
    uint8_t gpio_dout = 4;

    // Sanity checks
	if (!((1 << unit) & CPU_I2S_ALL)) {
		return driver_operation_error(I2S_DRIVER, I2S_ERR_INVALID_UNIT, NULL);
	}

	mtx_lock(&i2s[unit].mtx);

	// If unit is setup, remove first
	if (i2s[unit].setup) {
		i2s_driver_uninstall(unit);

		if (unit == 0) {
			periph_module_disable(PERIPH_I2S0_MODULE);
		} else {
			periph_module_disable(PERIPH_I2S1_MODULE);
		}

		i2s[unit].setup = 0;
	}

    // Lock resources
    i2s_resources_t resources;

    resources.ws = gpio_ws;
    resources.bck = gpio_bck;
    resources.din = gpio_din;
    resources.dout = gpio_dout;

    if ((error = i2s_lock_resources(unit, &resources))) {
    	mtx_unlock(&i2s[unit].mtx);

		return error;
	}

#if 0
    // Setup
    int buff_len = 0;
    i2c_config_t conf;

    conf.sda_io_num = sda;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = scl;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;

    if (mode == I2C_MASTER) {
        conf.mode = I2C_MODE_MASTER;
        conf.master.clk_speed = speed * 1000;
        buff_len = 0;
    } else {
    	conf.mode = I2C_MODE_SLAVE;
    	conf.slave.addr_10bit_en = addr10_en;
    	conf.slave.slave_addr = addr;
    	buff_len = 1024;
    }

    i2c_param_config(unit, &conf);
    i2c_driver_install(unit, conf.mode, buff_len, buff_len, 0);
#endif

    i2s[unit].mode = mode;
    i2s[unit].setup = 1;

	mtx_unlock(&i2s[unit].mtx);

    syslog(LOG_INFO,
        "i2s%u at pins ws=%s%d/bck=%s%d/din=%s%d/dout=%s%d", unit,
        gpio_portname(gpio_ws), gpio_name(gpio_ws),
        gpio_portname(gpio_bck), gpio_name(gpio_bck),
        gpio_portname(gpio_din), gpio_name(gpio_din),
        gpio_portname(gpio_dout), gpio_name(gpio_dout)
    );

    return NULL;
}

driver_error_t *i2s_lua_start(int unit) {
	driver_error_t *error;

	// Sanity checks
	if ((error = i2s_check(unit))) {
		return error;
	}

#if 0
	if (i2c[unit].mode != I2C_MASTER) {
		return driver_operation_error(I2C_DRIVER, I2C_ERR_INVALID_OPERATION, NULL);
	}
#endif

	mtx_lock(&i2s[unit].mtx);

	i2s_start(unit);

	mtx_unlock(&i2s[unit].mtx);

	return NULL;
}

driver_error_t *i2s_lua_stop(int unit) {
	driver_error_t *error;

	// Sanity checks
	if ((error = i2s_check(unit))) {
		return error;
	}

	mtx_lock(&i2s[unit].mtx);

	i2s_stop(unit);

	mtx_unlock(&i2s[unit].mtx);

	return NULL;
}

DRIVER_REGISTER(I2S,i2s,i2s_locks,i2s_lua_init,NULL);
