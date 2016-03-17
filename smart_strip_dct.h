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

#include <stdint.h>
#include "power_strip.h"

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************
 *                     Macros
 ******************************************************/

/******************************************************
 *                    Constants
 ******************************************************/

/******************************************************
 *                   Enumerations
 ******************************************************/


/******************************************************
 *                 Type Definitions
 ******************************************************/

/******************************************************
 *                    Structures
 ******************************************************/
typedef struct {
	wiced_bool_t	configured;
	uint8_t		is_set_name;
	char		dev_name[32];
	uint8_t		activeflag;
    power_strip_config_t power_strip_config;
} smart_strip_app_dct_t;

/******************************************************
 *                 Global Variables
 ******************************************************/

/******************************************************
 *               Function Declarations
 ******************************************************/
wiced_result_t load_app_data(smart_strip_app_dct_t* app_dct);
void store_app_data(smart_strip_app_dct_t* app_dct);
wiced_result_t load_wifi_data(platform_dct_wifi_config_t* wifi_dct);
void store_wifi_data(platform_dct_wifi_config_t* wifi_dct);

#ifdef __cplusplus
} /*extern "C" */
#endif
