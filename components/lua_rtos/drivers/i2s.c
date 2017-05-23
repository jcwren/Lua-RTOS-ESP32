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

#include <stdlib.h>
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
DRIVER_REGISTER_ERROR(I2S, i2s, DriverInstall, "driver install", I2S_ERR_DRIVER_INSTALL);
DRIVER_REGISTER_ERROR(I2S, i2s, DriverSetPin, "driver set_pin", I2S_ERR_DRIVER_SET_PIN);

// i2s info needed by driver
static i2s_t i2s[CPU_LAST_I2S + 1];

/*
 * Helper functions
 */
static driver_error_t *i2s_lock_pin(int unit, i2s_pin_config_t *pin) {
    driver_unit_lock_error_t *lock_error = NULL;

    if ((lock_error = driver_lock(I2S_DRIVER, unit, GPIO_DRIVER, pin->bck_io_num))) {
        return driver_lock_error(I2S_DRIVER, lock_error);
    }

    if ((lock_error = driver_lock(I2S_DRIVER, unit, GPIO_DRIVER, pin->ws_io_num))) {
        return driver_lock_error(I2S_DRIVER, lock_error);
    }

    if (pin->data_out_num != -1) {
        if ((lock_error = driver_lock(I2S_DRIVER, unit, GPIO_DRIVER, pin->data_out_num))) {
            return driver_lock_error(I2S_DRIVER, lock_error);
        }
    }

    if (pin->data_in_num != -1) {
        if ((lock_error = driver_lock(I2S_DRIVER, unit, GPIO_DRIVER, pin->data_in_num))) {
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

driver_error_t *i2s_lua_setup(int unit, const i2s_config_t *config, i2s_pin_config_t *pin, int queue_size) {
    driver_error_t *error;
    i2s_t *i2s_p;

    // Sanity checks
    if (!((1 << unit) & CPU_I2S_ALL)) {
        return driver_operation_error(I2S_DRIVER, I2S_ERR_INVALID_UNIT, NULL);
    }

    i2s_p = &i2s [unit];

    mtx_lock(&i2s_p->mtx);

    // If unit is already setup, remove first
    if (i2s_p->setup) {
        i2s_driver_uninstall(unit);

        if (unit == 0) {
            periph_module_disable(PERIPH_I2S0_MODULE);
        } else {
            periph_module_disable(PERIPH_I2S1_MODULE);
        }

        if (i2s_p->i2s_queue) {
          free (i2s_p->i2s_queue);
          i2s_p->i2s_queue = NULL;
        }

        i2s_p->setup = 0;
    }

    void *i2s_queue = calloc (queue_size, sizeof (uint8_t));
    if (!i2s_queue) {
        mtx_unlock(&i2s_p->mtx);
        return driver_operation_error(I2S_DRIVER, I2S_ERR_NOT_ENOUGH_MEMORY, NULL);
    }

    if ((error = i2s_lock_pin(unit, pin))) {
        mtx_unlock(&i2s_p->mtx);
        return error;
    }

    if (i2s_driver_install (unit, config, queue_size, i2s_queue) != ESP_OK) {
        mtx_unlock(&i2s_p->mtx);
        return driver_operation_error(I2S_DRIVER, I2S_ERR_DRIVER_INSTALL, NULL);
    }

    if (i2s_set_pin (unit, pin) != ESP_OK) {
        i2s_driver_uninstall (unit);
        mtx_unlock(&i2s_p->mtx);
        return driver_operation_error(I2S_DRIVER, I2S_ERR_DRIVER_SET_PIN, NULL);
    }

    i2s_p->config.mode                 = config->mode;
    i2s_p->config.sample_rate          = config->sample_rate;
    i2s_p->config.bits_per_sample      = config->bits_per_sample;
    i2s_p->config.channel_format       = config->channel_format;
    i2s_p->config.communication_format = config->communication_format;
    i2s_p->config.intr_alloc_flags     = config->intr_alloc_flags;
    i2s_p->config.dma_buf_count        = config->dma_buf_count;
    i2s_p->config.dma_buf_len          = config->dma_buf_len;

    i2s_p->pin.bck_io_num   = pin->bck_io_num;
    i2s_p->pin.ws_io_num    = pin->ws_io_num;
    i2s_p->pin.data_out_num = pin->data_out_num;
    i2s_p->pin.data_in_num  = pin->data_in_num;

    i2s_p->queue_size = queue_size;
    i2s_p->i2s_queue  = i2s_queue;

    i2s_p->setup = 1;

    mtx_unlock(&i2s_p->mtx);

    syslog(LOG_INFO,
        "i2s%u at pins bck=%s%d/ws=%s%d/dout=%s%d/din=%s%d", unit,
        gpio_portname(pin->bck_io_num), gpio_name(pin->bck_io_num),
        gpio_portname(pin->ws_io_num), gpio_name(pin->ws_io_num),
        gpio_portname(pin->data_out_num), gpio_name(pin->data_out_num),
        gpio_portname(pin->data_in_num), gpio_name(pin->data_in_num)
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
