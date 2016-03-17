/**
 * Copyright 2014, Broadcom Corporation
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 */

/** @file
 *
 */
#include "wiced.h"
#include "power_strip.h"
#include "wiced_time.h"
#include "wiced_rtos.h"
#include "wiced_utilities.h"
#include "wwd_constants.h"
#include "smart_strip_dct.h"

/******************************************************
 *                      Macros
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



/******************************************************
 *               Static Function Declarations
 ******************************************************/


/******************************************************
 *               Variable Definitions
 ******************************************************/
static power_strip_t *power_strip;
static power_strip_config_t *strip_config;

/******************************************************
 *               Function Definitions
 ******************************************************/
wiced_result_t power_strip_init(power_strip_t **power_strip_arg, power_strip_config_t *power_strip_config)
{
	int i;

	power_strip = (power_strip_t *) malloc_named("power_strip", sizeof(power_strip_t));
    if (power_strip == NULL) {
        return WICED_ERROR;
    }
	memset(power_strip, 0, sizeof(power_strip_t));

	power_strip->jack_count = power_strip_config->jack_count;
	for(i = 0; i < power_strip->jack_count; i++) {
		power_strip->jack_list[i].jack_no = power_strip_config->jack_config[i].jack_no;
		power_strip->jack_list[i].relay_io = power_strip_config->jack_config[i].relay_io;
		power_strip->jack_list[i].status = power_strip_config->jack_config[i].status;
		strncpy(power_strip->jack_list[i].jack_name, power_strip_config->jack_config[i].jack_name, 32);
		wiced_gpio_init(power_strip->jack_list[i].relay_io, OUTPUT_PUSH_PULL);
		set_jack_status(power_strip->jack_list[i].jack_no, power_strip->jack_list[i].status);
	}

	*power_strip_arg = power_strip;
	strip_config = power_strip_config;
	
	return WICED_SUCCESS;
}

static void sync_jack_state(uint8_t jack_no)
{
	int i;

	for(i = 0; i < power_strip->jack_count; i++) {
		if(jack_no == power_strip->jack_list[i].jack_no) {
			strip_config->jack_config[i].status = power_strip->jack_list[i].status;
		}
	}
}

#if 0
void switch_jack_status(jack_t *jack)
{
	jack_status_t status = get_jack_status(jack);

	if(status == JACK_STATUS_ON) {
		set_jack_status(jack, JACK_STATUS_OFF);
	} else if(status == JACK_STATUS_OFF) {
		set_light_status(jack, JACK_STATUS_ON);
	}
	power_strip->function(jack);
	return;
}
#endif
int get_jack_count()
{
	return power_strip->jack_count;
}

static jack_t *find_jack_by_no(uint8_t jack_no)
{
	int i;

	for(i = 0; i < power_strip->jack_count; i++) {
		if(jack_no == power_strip->jack_list[i].jack_no) {
			return &power_strip->jack_list[i];
		}
	}
	return NULL;
}

void set_jack_status(uint8_t jack_no, jack_status_t status)
{
	jack_t *jack;
	
	jack = find_jack_by_no(jack_no);

	if(jack != NULL) {
		if(status == JACK_STATUS_ON) {
			wiced_gpio_output_high(jack->relay_io);
			jack->status = JACK_STATUS_ON;
			WPRINT_APP_INFO(("jack_%d is on\n", jack->jack_no));
		}
		else if(status == JACK_STATUS_OFF) {
			wiced_gpio_output_low(jack->relay_io );
			jack->status = JACK_STATUS_OFF;
			WPRINT_APP_INFO(("jack_%d is off\n", jack->jack_no));
		}
		sync_jack_state(jack_no);
	}
	return;
}

jack_status_t get_jack_status(uint8_t jack_no)
{
	jack_t *jack;
	
	jack = find_jack_by_no(jack_no);
	if(jack != NULL) {
		return jack->status;
	}
	return -1;
}

uint8_t get_all_jack_status()
{
	int i;
	uint8_t value = 0;
	
	for(i = 0; i < power_strip->jack_count; i++) {
		value |= (get_jack_status(power_strip->jack_list[i].jack_no)) << (power_strip->jack_list[i].jack_no - 1);
	}
	return value;
}
