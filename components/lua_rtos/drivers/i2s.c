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
driver_unit_lock_t i2s_locks [CPU_LAST_I2S + 1];

DRIVER_REGISTER_ERROR (I2S, i2s, NotSetup, "is not setup", I2S_ERR_IS_NOT_SETUP);
DRIVER_REGISTER_ERROR (I2S, i2s, InvalidUnit, "invalid unit", I2S_ERR_INVALID_UNIT);
DRIVER_REGISTER_ERROR (I2S, i2s, NotEnoughtMemory, "not enough memory", I2S_ERR_NOT_ENOUGH_MEMORY);
DRIVER_REGISTER_ERROR (I2S, i2s, DriverInstall, "driver install", I2S_ERR_DRIVER_INSTALL);
DRIVER_REGISTER_ERROR (I2S, i2s, DriverSetPin, "driver set_pin", I2S_ERR_DRIVER_SET_PIN);
DRIVER_REGISTER_ERROR (I2S, i2s, DriverError, "driver error", I2S_ERR_DRIVER_ERROR);
DRIVER_REGISTER_ERROR (I2S, i2s, SizeGtLen, "size > string len", I2S_ERR_SIZE_GT_LEN);
DRIVER_REGISTER_ERROR (I2S, i2s, BadSampleSize, "bad sample length", I2S_ERR_BAD_SAMPLE_LENGTH);

// i2s info needed by driver
static i2s_t i2s [CPU_LAST_I2S + 1];

/*
 * Helper functions
 */
static driver_error_t *i2s_lock_pin (int unit, i2s_pin_config_t *pin) {
  driver_unit_lock_error_t *lock_error = NULL;

  if ((lock_error = driver_lock (I2S_DRIVER, unit, GPIO_DRIVER, pin->bck_io_num)))
    return driver_lock_error (I2S_DRIVER, lock_error);

  if ((lock_error = driver_lock (I2S_DRIVER, unit, GPIO_DRIVER, pin->ws_io_num)))
    return driver_lock_error (I2S_DRIVER, lock_error);

  if (pin->data_out_num != -1)
    if ((lock_error = driver_lock (I2S_DRIVER, unit, GPIO_DRIVER, pin->data_out_num)))
      return driver_lock_error (I2S_DRIVER, lock_error);

  if (pin->data_in_num != -1)
    if ((lock_error = driver_lock (I2S_DRIVER, unit, GPIO_DRIVER, pin->data_in_num)))
      return driver_lock_error (I2S_DRIVER, lock_error);

  return NULL;
}

static driver_error_t *i2s_check (int unit) {
  if (!((1 << unit) & CPU_I2S_ALL))
    return driver_operation_error (I2S_DRIVER, I2S_ERR_INVALID_UNIT, NULL);

  if (!i2s [unit].setup)
    return driver_operation_error (I2S_DRIVER, I2S_ERR_IS_NOT_SETUP, NULL);

  return NULL;
}

/*
 * Operation functions
 */

int i2s_lua_exists (int unit) {
  return ((unit >= CPU_FIRST_I2S) && (unit <= CPU_LAST_I2S));
}

int i2s_lua_is_setup (int unit) {
  return i2s_lua_exists (unit) && i2s [unit].setup;
}

int i2s_lua_get_pushpop_size (int unit) {
  if (!i2s_lua_is_setup (unit))
    return 0;

  return i2s [unit].bytes_per_pushpop;
}

void i2s_lua_init () {
  int i;

  for (i = 0; i < CPU_LAST_I2S; i++)
    mtx_init (&i2s [i].mtx, NULL, NULL, 0);
}

driver_error_t *i2s_lua_setup (int unit, const i2s_config_t *config, i2s_pin_config_t *pin, int evtqueue_size) {
  driver_error_t *error;
  i2s_t *i2s_p;
  void *evtqueue = NULL;

  if (!((1 << unit) & CPU_I2S_ALL))
    return driver_operation_error (I2S_DRIVER, I2S_ERR_INVALID_UNIT, NULL);

  i2s_p = &i2s [unit];

  mtx_lock (&i2s_p->mtx);

  if (i2s_p->setup) {
    i2s_driver_uninstall (unit);

    periph_module_disable ((unit == 0) ? PERIPH_I2S0_MODULE : PERIPH_I2S1_MODULE);

    if (i2s_p->evtqueue) {
      free (i2s_p->evtqueue);
      i2s_p->evtqueue = NULL;
    }

    i2s_p->setup = 0;
  }

  if (evtqueue_size) {
    if (!(evtqueue = calloc (evtqueue_size, sizeof (i2s_event_t)))) {
      mtx_unlock (&i2s_p->mtx);
      return driver_operation_error (I2S_DRIVER, I2S_ERR_NOT_ENOUGH_MEMORY, NULL);
    }
  }

  if ((error = i2s_lock_pin (unit, pin))) {
    mtx_unlock (&i2s_p->mtx);
    return error;
  }

  if (i2s_driver_install (unit, config, evtqueue_size, &evtqueue) != ESP_OK) {
    mtx_unlock (&i2s_p->mtx);
    return driver_operation_error (I2S_DRIVER, I2S_ERR_DRIVER_INSTALL, NULL);
  }

  if (i2s_set_pin (unit, pin) != ESP_OK) {
    i2s_driver_uninstall (unit);
    mtx_unlock (&i2s_p->mtx);
    return driver_operation_error (I2S_DRIVER, I2S_ERR_DRIVER_SET_PIN, NULL);
  }

  i2s_p->config.mode                 = config->mode;
  i2s_p->config.sample_rate          = config->sample_rate;
  i2s_p->config.bits_per_sample      = config->bits_per_sample;
  i2s_p->config.channel_format       = config->channel_format;
  i2s_p->config.communication_format = config->communication_format;
  i2s_p->config.intr_alloc_flags     = config->intr_alloc_flags;
  i2s_p->config.dma_buf_count        = config->dma_buf_count;
  i2s_p->config.dma_buf_len          = config->dma_buf_len;

  i2s_p->num_channels = (config->channel_format < I2S_CHANNEL_FMT_ONLY_RIGHT) ? 2 : 1;
  i2s_p->bytes_per_sample = ((i2s_p->config.bits_per_sample + 15) / 16) * 2;
  i2s_p->bytes_per_pushpop = i2s_p->bytes_per_sample * i2s_p->num_channels;

  i2s_p->pin.bck_io_num   = pin->bck_io_num;
  i2s_p->pin.ws_io_num    = pin->ws_io_num;
  i2s_p->pin.data_out_num = pin->data_out_num;
  i2s_p->pin.data_in_num  = pin->data_in_num;

  i2s_p->evtqueue_size = evtqueue_size;
  i2s_p->evtqueue      = evtqueue;

  i2s_p->setup = 1;

  mtx_unlock (&i2s_p->mtx);

  syslog (LOG_INFO,
      "i2s%u at pins bck=%s%d/ws=%s%d/dout=%s%d/din=%s%d", unit,
      gpio_portname (pin->bck_io_num), gpio_name (pin->bck_io_num),
      gpio_portname (pin->ws_io_num), gpio_name (pin->ws_io_num),
      gpio_portname (pin->data_out_num), gpio_name (pin->data_out_num),
      gpio_portname (pin->data_in_num), gpio_name (pin->data_in_num)
      );

  return NULL;
}

driver_error_t *i2s_lua_start (int unit) {
  driver_error_t *error;
  i2s_t *i2s_p;

  if ((error = i2s_check (unit)))
    return error;

  i2s_p = &i2s [unit];

  mtx_lock (&i2s_p->mtx);

  if (i2s_start (unit) != ESP_OK)
    error = driver_operation_error (I2S_DRIVER, I2S_ERR_DRIVER_ERROR, NULL);

  mtx_unlock (&i2s_p->mtx);

  return error;
}

driver_error_t *i2s_lua_stop (int unit) {
  driver_error_t *error;
  i2s_t *i2s_p;

  if ((error = i2s_check (unit)))
    return error;

  i2s_p = &i2s [unit];

  mtx_lock (&i2s_p->mtx);

  if (i2s_stop (unit) != ESP_OK)
    error = driver_operation_error (I2S_DRIVER, I2S_ERR_DRIVER_ERROR, NULL);

  mtx_unlock (&i2s_p->mtx);

  return error;
}

driver_error_t *i2s_lua_write (int unit, void *src, size_t size, TickType_t ticks_to_wait, int *bytesWritten) {
  driver_error_t *error;
  i2s_t *i2s_p;

  if ((error = i2s_check (unit)))
    return error;

  i2s_p = &i2s [unit];

  mtx_lock (&i2s_p->mtx);

  if ((*bytesWritten = i2s_write_bytes (unit, src, size, ticks_to_wait)) == ESP_FAIL)
    error = driver_operation_error (I2S_DRIVER, I2S_ERR_DRIVER_ERROR, NULL);

  mtx_unlock (&i2s_p->mtx);

  return error;
}

driver_error_t *i2s_lua_read (int unit, void *dest, size_t size, TickType_t ticks_to_wait, int *bytesRead) {
  driver_error_t *error;
  i2s_t *i2s_p;

  if ((error = i2s_check (unit)))
    return error;

  i2s_p = &i2s [unit];

  mtx_lock (&i2s_p->mtx);

  if ((*bytesRead = i2s_read_bytes (unit, dest, size, ticks_to_wait)) == ESP_FAIL)
    error = driver_operation_error (I2S_DRIVER, I2S_ERR_DRIVER_ERROR, NULL);

  mtx_unlock (&i2s_p->mtx);

  return NULL;
}

driver_error_t *i2s_lua_push (int unit, void *sample, TickType_t ticks_to_wait, int *bytesWritten) {
  driver_error_t *error;
  i2s_t *i2s_p;

  if ((error = i2s_check (unit)))
    return error;

  i2s_p = &i2s [unit];

  mtx_lock (&i2s_p->mtx);

  if ((*bytesWritten = i2s_push_sample (unit, sample, ticks_to_wait)) == ESP_FAIL)
    error = driver_operation_error (I2S_DRIVER, I2S_ERR_DRIVER_ERROR, NULL);

  mtx_unlock (&i2s_p->mtx);

  return error;
}

driver_error_t *i2s_lua_pop (int unit, void *sample, TickType_t ticks_to_wait, int *bytesRead) {
  driver_error_t *error;
  i2s_t *i2s_p;

  if ((error = i2s_check (unit)))
    return error;

  i2s_p = &i2s [unit];

  mtx_lock (&i2s_p->mtx);

  if ((*bytesRead = i2s_pop_sample (unit, sample, ticks_to_wait)) == ESP_FAIL)
    error = driver_operation_error (I2S_DRIVER, I2S_ERR_DRIVER_ERROR, NULL);

  mtx_unlock (&i2s_p->mtx);

  return error;
}

driver_error_t *i2s_lua_zerobuf (int unit) {
  driver_error_t *error;
  i2s_t *i2s_p;

  if ((error = i2s_check (unit)))
    return error;

  i2s_p = &i2s [unit];

  mtx_lock (&i2s_p->mtx);

  if (i2s_zero_dma_buffer (unit) != ESP_OK)
    error = driver_operation_error (I2S_DRIVER, I2S_ERR_DRIVER_ERROR, NULL);

  mtx_unlock (&i2s_p->mtx);

  return error;
}

driver_error_t *i2s_lua_setrate (int unit, uint32_t rate) {
  driver_error_t *error;
  i2s_t *i2s_p;

  if ((error = i2s_check (unit)))
    return error;

  i2s_p = &i2s [unit];

  mtx_lock (&i2s_p->mtx);

  if (i2s_set_sample_rates (unit, rate) != ESP_OK)
    error = driver_operation_error (I2S_DRIVER, I2S_ERR_DRIVER_ERROR, NULL);

  mtx_unlock (&i2s_p->mtx);

  return error;
}

driver_error_t *i2s_lua_setclk (int unit, uint32_t rate, int bits, int channel) {
  driver_error_t *error;
  i2s_t *i2s_p;

  if ((error = i2s_check (unit)))
    return error;

  i2s_p = &i2s [unit];

  mtx_lock (&i2s->mtx);

  i2s_p->config.bits_per_sample = bits;
  i2s_p->num_channels = (channel == 2) ? 2 : 1;
  i2s_p->bytes_per_sample = ((i2s_p->config.bits_per_sample + 15) / 16) * 2;
  i2s_p->bytes_per_pushpop = i2s_p->bytes_per_sample * i2s_p->num_channels;

  if (i2s_set_clk (unit, rate, i2s_p->config.bits_per_sample, i2s_p->num_channels) != ESP_OK)
    error = driver_operation_error (I2S_DRIVER, I2S_ERR_DRIVER_ERROR, NULL);

  mtx_unlock (&i2s_p->mtx);

  return error;
}

driver_error_t *i2s_lua_dacmode (int dacmode) {
  driver_error_t *error = NULL;

  if (i2s_set_dac_mode (dacmode) != ESP_OK)
    error = driver_operation_error (I2S_DRIVER, I2S_ERR_DRIVER_ERROR, NULL);

  return error;
}

DRIVER_REGISTER (I2S, i2s, i2s_locks, i2s_lua_init, NULL);