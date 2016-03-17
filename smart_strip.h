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

#include "power_strip.h"
#include "smart_strip_dct.h"

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************
 *                      Macros
 ******************************************************/

/******************************************************
 *                    Constants
 ******************************************************/


/******************************************************
 *                   Enumerations
 ******************************************************/
enum {
	DEVICE_CONNECTING = 40,
	DEVICE_ACTIVE_DONE,
	DEVICE_ACTIVE_FAIL,
	DEVICE_CONNECT_SERVER_FAIL
};

/******************************************************
 *                 Type Definitions
 ******************************************************/
typedef wiced_result_t (*parse_socket_msg_fun_t)(char *, uint16_t);

typedef wiced_result_t (*parse_uart_msg_fun_t)();

//typedef void (*uart_receive_handler_t)(void);


/******************************************************
 *                    Structures
 ******************************************************/
typedef struct {
#if 0
	wiced_bool_t configured;
	uint8_t dev_name[32];
	wiced_mac_t mac_addr;
	uint8_t activeflag;
	uint8_t is_set_name;
#endif
	uint8_t device_status;
	uint8_t ping_status;
	uint8_t tcp_conn_state;
	uint32_t retry_ms;

	wiced_udp_socket_t udp_socket;
	wiced_tcp_socket_t tcp_socket;
	wiced_ip_address_t server_ip;	
	wiced_timed_event_t tcp_conn_event;
	wiced_timed_event_t beacon_event;

	power_strip_t *power_strip;
	smart_strip_app_dct_t app_dct;
	platform_dct_wifi_config_t wifi_dct;
} dev_info_t;

/******************************************************
 *                 Global Variables
 ******************************************************/

/******************************************************
 *               Function Declarations
 ******************************************************/

#ifdef __cplusplus
} /*extern "C" */
#endif
