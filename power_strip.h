/*
 * Copyright 2014, Broadcom Corporation
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 */
#pragma once

#include "wiced.h"

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************
 *                      Macros
 ******************************************************/

/******************************************************
 *                    Constants
 ******************************************************/
#define MAX_JACK_COUNT 6

/******************************************************
 *                   Enumerations
 ******************************************************/
typedef enum {
	JACK_STATUS_ON = 0x00,
	JACK_STATUS_OFF = 0x01,
}jack_status_t;

typedef enum {
    RELAY_GPIO_1 = WICED_GPIO_33,
    RELAY_GPIO_2 = WICED_GPIO_34,
    RELAY_GPIO_3 = WICED_GPIO_35,
    RELAY_GPIO_4 = WICED_GPIO_36,
    RELAY_GPIO_5 = WICED_GPIO_37,
    RELAY_GPIO_6 = WICED_GPIO_38
}relay_gpio_t;

/******************************************************
 *                 Type Definitions
 ******************************************************/
typedef void (*strip_handler_t)(void *arg);

/******************************************************
 *                    Structures
 ******************************************************/
typedef struct {
	uint8_t jack_no;
	relay_gpio_t relay_io;
	jack_status_t status;
	char jack_name[32];
}jack_config_t;

typedef struct {
	uint8_t jack_count;
	jack_config_t jack_config[MAX_JACK_COUNT];
}power_strip_config_t;

typedef struct jack {
	uint8_t jack_no;
	relay_gpio_t relay_io;
	jack_status_t status;
	char jack_name[32];
}jack_t;

typedef struct power_strip {
	uint32_t jack_count;
	jack_t jack_list[MAX_JACK_COUNT];
}power_strip_t;

/******************************************************
 *                 Global Variables
 ******************************************************/

/******************************************************
 *               Function Declarations
 ******************************************************/
wiced_result_t power_strip_init(power_strip_t **power_strip_arg, power_strip_config_t *power_strip_config);
void set_jack_status(uint8_t jack_no, jack_status_t status);
jack_status_t get_jack_status(uint8_t jack_no);
int get_jack_count();
uint8_t get_all_jack_status();

#ifdef __cplusplus
} /* extern "C" */
#endif
