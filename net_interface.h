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
#include "wiced_rtos.h"
#include "wiced_utilities.h"

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************
 *                      Macros
 ******************************************************/
#define	MSG_BST_CONTROL "bst"
#define MSG_HANDSHAKE_SYN "ff1",
#define MSG_HANDSHAKE_ASK "ff2"

#define MSG_HEAD_LEN 20
/******************************************************
 *                    Constants
 ******************************************************/


/******************************************************
 *                   Enumerations
 ******************************************************/
enum net_mode{
	NET_MODE_CHAIN,
	NET_MODE_STAR,
};

enum dev_type{
	DEV_TYPE_MASTER = 0x01,
	DEV_TYPE_SLAVE = 0x02,
};

enum msg_flag{
	MSG_CONTROL_PANEL = 0x00,
	MSG_MOBILE_DEVICE = 0x01,
};

enum obj_type{
	OBJ_TYPE_LIGHT = 0x01,
	OBJ_TYPE_CURTAIN = 0x02,
};

enum ctrl_flag{
	CTRL_FLAG_NONE = 0x00,
	CTRL_FLAG_PANEL = 0x01,
	CTRL_FLAG_PHONE = 0x02,
};

typedef enum {
	DEVICE_GET_INFO = 0x01,
	LIGHT_FUN_SET_STATE = 0x11,
	LIGHT_FUN_GET_STATE = 0x12,
	LIGHT_FUN_REPORT_STATE = 0x13,
	CURTAIN_FUN_SET_POS = 0x21,
	CURTAIN_FUN_GET_POS = 0x22,
	CURTAIN_FUN_REPORT_POS = 0x23,
}light_fun_t;

/******************************************************
 *                 Type Definitions
 ******************************************************/
 

/******************************************************
 *                    Structures
 ******************************************************/
#pragma pack(1)
	
typedef uint8_t msg_type_t[3];

typedef struct {
	wiced_mac_t mac_addr;
	uint8_t obj_type;
	uint8_t obj_no;
} obj_id_t;

typedef struct {
	msg_type_t	msg_type;
	uint8_t 	data_len;
	uint32_t 	nonce;
	obj_id_t	obj_id;
	uint8_t 	fun_type;
	uint8_t		fun_data;
	uint8_t		flag;
	uint32_t	ip_addr;
	uint16_t	udp_port;
	uint8_t		reserved[3];
	uint8_t 	byte28;
	uint8_t 	byte29;
	uint8_t 	byte30;
	uint8_t 	byte31;
	uint8_t 	byte32;
	uint8_t 	byte33;
	uint8_t 	byte34;
	uint8_t 	byte35;
	uint8_t 	byte36;
	uint8_t 	byte37;
	uint8_t 	byte38;
	uint8_t 	byte39;
	uint8_t 	byte40;
	uint8_t 	byte41;
	uint8_t 	byte42;
	uint8_t 	byte43;
	uint8_t 	byte44;
	uint8_t 	byte45;
	uint8_t 	byte46;
	uint8_t 	byte47;
	uint8_t 	byte48;
	uint8_t 	byte49;
	uint8_t 	byte50;
	uint8_t 	byte51;
	uint8_t 	byte52;
	uint8_t 	byte53;
	uint8_t 	byte54;
	uint8_t 	byte55;
	uint8_t 	byte56;
	uint8_t 	byte57;
	uint8_t 	byte58;
	uint8_t 	byte59;
	uint8_t 	byte60;
} msg_t;

typedef struct {
	uint16_t	prefix;
	uint8_t 	data_len;
	uint16_t	cmd_type;
	uint8_t 	status;
} keypress_cmd_t;
	
#pragma pack()


/******************************************************
 *                 Global Variables
 ******************************************************/

/******************************************************
 *               Function Declarations
 ******************************************************/
wiced_result_t next_receive_enable();
wiced_result_t pre_receive_enable();
wiced_result_t user_receive_enable();
wiced_result_t send_udp_packet(wiced_udp_socket_t* socket, const wiced_ip_address_t* ip_addr, const uint16_t udp_port, char *buffer, uint16_t length);
wiced_result_t master_parse_socket_msg (const wiced_ip_address_t* ip_addr, const uint16_t udp_port, char* buffer, uint16_t buffer_length);
wiced_result_t slave_parse_socket_msg(const wiced_ip_address_t* ip_addr, const uint16_t udp_port, char *rx_data, uint16_t rx_data_length);
wiced_result_t send_to_pre_dev(char *buffer, uint16_t length);
wiced_result_t send_response(const wiced_ip_address_t* ip_addr, const uint16_t udp_port, char *buffer, uint16_t length);
wiced_result_t send_to_next_dev(char *buffer, uint16_t length);
wiced_result_t send_to_user(char *buffer, uint16_t length);

#ifdef __cplusplus
} /* extern "C" */
#endif
