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
#include "wiced_time.h"
#include "wiced_rtos.h"
#include "wiced_utilities.h"
#include "wwd_constants.h"
#include "net_interface.h"
#include "light_dev.h"
#include "curtain_dev.h"
#include "smart_home.h"

/******************************************************
 *                      Macros
 ******************************************************/
#define UDP_MAX_DATA_LENGTH         256
#define RX_WAIT_TIMEOUT        (1*SECONDS)
#define PORTNUM                (8088)           /* UDP port */
#define SEND_UDP_RESPONSE

//#define PRE_DEV_IP_ADDRESS MAKE_IPV4_ADDRESS(192,168,0,1)
#define UDP_BROADCAST_ADDR MAKE_IPV4_ADDRESS(192,168,1,255)

/******************************************************
 *                    Constants
 ******************************************************/
const msg_type_t msg_bst_control = {0x42, 0x53, 0x54};
const msg_type_t msg_handshake_syn = {0xff, 0xff, 0x01};
const msg_type_t msg_handshake_ack = {0xff, 0xff, 0x02};


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
static wiced_result_t sta_conn_check(void *arg);
static wiced_result_t test_event_fun(void *arg);
static wiced_result_t pre_receive_callback(void *socket);
static wiced_result_t next_receive_callback(void *socket);
static wiced_result_t ap_receive_callback(void *socket);
static wiced_result_t sta_receive_callback(void *socket);
static wiced_result_t user_receive_callback(void *socket);
static wiced_result_t light_parse_msg(const wiced_ip_address_t* ip_addr, const uint16_t udp_port, light_t *light, msg_t *msgp);
static wiced_result_t curtain_parse_msg(const wiced_ip_address_t* ip_addr, const uint16_t udp_port, curtain_t *curtain, msg_t *msgp);
static wiced_result_t handshake(wiced_udp_socket_t* socket, wiced_ip_address_t *ip_addr, const msg_type_t msg_type);
//static void report_curtain_pos(curtain_t *curtain);
//static void report_light_status(light_t *light);
static wiced_bool_t msg_type_cmp(const msg_type_t msg_type1, const msg_type_t msg_type2);
static void link_up( void );
static void link_down( void );
static wiced_bool_t mac_addr_cmp(wiced_mac_t addr1, wiced_mac_t addr2);

/******************************************************
 *               Variable Definitions
 ******************************************************/
static const wiced_ip_setting_t ap_ip_settings =
{
	INITIALISER_IPV4_ADDRESS( .ip_address, MAKE_IPV4_ADDRESS( 192,168,	1,	1 ) ),
	INITIALISER_IPV4_ADDRESS( .netmask,    MAKE_IPV4_ADDRESS( 255,255,255,	0 ) ),
	INITIALISER_IPV4_ADDRESS( .gateway,    MAKE_IPV4_ADDRESS( 192,168,	1,	1 ) ),
};

//wiced_ip_address_t INITIALISER_IPV4_ADDRESS( pre_dev_addr, PRE_DEV_IP_ADDRESS );
wiced_ip_address_t INITIALISER_IPV4_ADDRESS( udp_broadcast_addr, UDP_BROADCAST_ADDR );

//static wiced_timed_event_t process_sta_rx_event;
//static wiced_timed_event_t process_ap_rx_event;
static wiced_timed_event_t sta_conn_check_event;
static wiced_timed_event_t test_event;
static wiced_timed_event_t udp_delay_event;
static wiced_udp_socket_t  next_socket;
static wiced_udp_socket_t  pre_socket;
static wiced_udp_socket_t  user_socket;
//static wiced_thread_t sta_receive_thread;
static int reconnected_status = 0;
static int alive_timeout = 0;
//static wiced_ip_address_t ctrl_ip_addr;
//static int cur_ctrl_flag;

static const wiced_mac_t broadcast_mac =
{
    .octet = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }
};

extern glob_info_t this_dev;
/******************************************************
 *               Function Definitions
 ******************************************************/
wiced_result_t master_receive_enable()
{
	if(this_dev.net_mode == NET_MODE_CHAIN) {
		if(wiced_network_is_up(WICED_AP_INTERFACE) == WICED_FALSE) {
			/* Bring up the softAP interface */
	    	wiced_network_up(WICED_AP_INTERFACE, WICED_USE_INTERNAL_DHCP_SERVER, &ap_ip_settings);
		}
		
		/* Create UDP socket */
		if ( wiced_udp_create_socket( &user_socket, USER_PORT, WICED_AP_INTERFACE ) != WICED_SUCCESS )
		{
			WPRINT_APP_INFO( ("User UDP socket creation failed\n") );
		}
		
		/* Create UDP socket */
		if ( wiced_udp_create_socket( &next_socket, PORTNUM, WICED_AP_INTERFACE ) != WICED_SUCCESS )
		{
			WPRINT_APP_INFO( ("UDP socket creation failed\n") );
		}
		
		/* Register a function to process received UDP packets */
		wiced_udp_register_callbacks(&user_socket, user_receive_callback);
		wiced_udp_register_callbacks(&next_socket, next_receive_callback);
	} else {
		/* Register callbacks */
		wiced_network_register_link_callback( link_up, link_down );
		
		/* Bring network up in STA mode */
		wiced_network_up( WICED_STA_INTERFACE, WICED_USE_EXTERNAL_DHCP_SERVER, NULL );
		wiced_rtos_register_timed_event( &sta_conn_check_event, WICED_NETWORKING_WORKER_THREAD, &sta_conn_check, 1*SECONDS, 0 );
	}
	return WICED_SUCCESS;
}

wiced_result_t slave_receive_enable()
{
	/* Register callbacks */
	wiced_network_register_link_callback( link_up, link_down );
	
	/* Bring network up in STA mode */
	wiced_network_up( WICED_STA_INTERFACE, WICED_USE_EXTERNAL_DHCP_SERVER, NULL );
	wiced_rtos_register_timed_event( &sta_conn_check_event, WICED_NETWORKING_WORKER_THREAD, &sta_conn_check, 1*SECONDS, 0 );
	if(this_dev.net_mode == NET_MODE_CHAIN) {
		if(wiced_network_is_up(WICED_AP_INTERFACE) == WICED_FALSE) {
			/* Bring up the softAP interface */
			wiced_network_up(WICED_AP_INTERFACE, WICED_USE_INTERNAL_DHCP_SERVER, &ap_ip_settings);
		}
		
		/* Create UDP socket */
		if ( wiced_udp_create_socket( &next_socket, PORTNUM, WICED_AP_INTERFACE ) != WICED_SUCCESS )
		{
			WPRINT_APP_INFO( ("UDP socket creation failed\n") );
		}
		
		/* Register a function to process received UDP packets */
		wiced_udp_register_callbacks(&next_socket, next_receive_callback);
	}
	return WICED_SUCCESS;
}

static wiced_result_t master_receive_callback(void *socket)
{
	wiced_packet_t* 		  packet;
	char*					  rx_data;
	static uint16_t 		  rx_data_length;
	uint16_t				  available_data_length;
	static wiced_ip_address_t udp_src_ip_addr;
	static uint16_t 		  udp_src_port;
	msg_t *msgp;

	/* Wait for UDP packet */
	wiced_result_t result = wiced_udp_receive( socket, &packet, RX_WAIT_TIMEOUT );

	if ( ( result == WICED_ERROR ) || ( result == WICED_TIMEOUT ) )
	{
		return result;
	}

	/* Get info about the received UDP packet */
	wiced_udp_packet_get_info( packet, &udp_src_ip_addr, &udp_src_port );

	/* Extract the received data from the UDP packet */
	wiced_packet_get_data( packet, 0, (uint8_t**) &rx_data, &rx_data_length, &available_data_length );

	/* Null terminate the received data, just in case the sender didn't do this */
	//rx_data[ rx_data_length ] = '\x0';

	WPRINT_APP_INFO ( ("UDP Rx: \"%.2x %.2x %.2x\" from %s IP %u.%u.%u.%u:%d\n", rx_data[0], rx_data[1], rx_data[2], (socket == &next_socket)?"AP":"STA",
																  (unsigned char) ( ( GET_IPV4_ADDRESS(udp_src_ip_addr) >> 24 ) & 0xff ),
																  (unsigned char) ( ( GET_IPV4_ADDRESS(udp_src_ip_addr) >> 16 ) & 0xff ),
																  (unsigned char) ( ( GET_IPV4_ADDRESS(udp_src_ip_addr) >>	8 ) & 0xff ),
																  (unsigned char) ( ( GET_IPV4_ADDRESS(udp_src_ip_addr) >>	0 ) & 0xff ),
																  udp_src_port ) );
	msgp = (msg_t *)rx_data;
	//WPRINT_APP_INFO(("msg->msg_type = %x \n", msg->msg_type));
	if(msg_type_cmp(msgp->msg_type, msg_bst_control)) {
		msgp->ip_addr = udp_src_ip_addr.ip.v4;
		master_parse_socket_msg(&udp_src_ip_addr, udp_src_port, rx_data, rx_data_length);
	}
	/* Delete the received packet, it is no longer needed */
	wiced_packet_delete( packet );

	return WICED_SUCCESS;
}

static wiced_result_t user_receive_callback(void *socket)
{
	wiced_packet_t* 		  packet;
	char*					  rx_data;
	static uint16_t 		  rx_data_length;
	uint16_t				  available_data_length;
	static wiced_ip_address_t udp_src_ip_addr;
	static uint16_t 		  udp_src_port;
	msg_t *msgp;

	/* Wait for UDP packet */
	wiced_result_t result = wiced_udp_receive( socket, &packet, RX_WAIT_TIMEOUT );

	if ( ( result == WICED_ERROR ) || ( result == WICED_TIMEOUT ) )
	{
		return result;
	}

	/* Get info about the received UDP packet */
	wiced_udp_packet_get_info( packet, &udp_src_ip_addr, &udp_src_port );

	/* Extract the received data from the UDP packet */
	wiced_packet_get_data( packet, 0, (uint8_t**) &rx_data, &rx_data_length, &available_data_length );

	/* Null terminate the received data, just in case the sender didn't do this */
	//rx_data[ rx_data_length ] = '\x0';

	WPRINT_APP_INFO ( ("UDP Rx: \"%.2x %.2x %.2x\" from %s IP %u.%u.%u.%u:%d\n", rx_data[0], rx_data[1], rx_data[2], "USER",
																  (unsigned char) ( ( GET_IPV4_ADDRESS(udp_src_ip_addr) >> 24 ) & 0xff ),
																  (unsigned char) ( ( GET_IPV4_ADDRESS(udp_src_ip_addr) >> 16 ) & 0xff ),
																  (unsigned char) ( ( GET_IPV4_ADDRESS(udp_src_ip_addr) >>	8 ) & 0xff ),
																  (unsigned char) ( ( GET_IPV4_ADDRESS(udp_src_ip_addr) >>	0 ) & 0xff ),
																  udp_src_port ) );
	msgp = (msg_t *)rx_data;
	//WPRINT_APP_INFO(("msg->msg_type = %x \n", msg->msg_type));
	if(msg_type_cmp(msgp->msg_type, msg_bst_control)) {
		msgp->flag = MSG_MOBILE_DEVICE;
		msgp->ip_addr = udp_src_ip_addr.ip.v4;
		msgp->udp_port = udp_src_port;
		send_to_next_dev(rx_data, rx_data_length);
	}
	/* Delete the received packet, it is no longer needed */
	wiced_packet_delete( packet );

	return WICED_SUCCESS;
}

static wiced_result_t slave_receive_callback(void *socket)
{
	wiced_packet_t* 		  packet;
	char*					  rx_data;
	static uint16_t 		  rx_data_length;
	uint16_t				  available_data_length;
	static wiced_ip_address_t udp_src_ip_addr;
	static uint16_t 		  udp_src_port;
	msg_t *msgp;

	/* Wait for UDP packet */
	wiced_result_t result = wiced_udp_receive( socket, &packet, RX_WAIT_TIMEOUT );

	if ( ( result == WICED_ERROR ) || ( result == WICED_TIMEOUT ) )
	{
		return result;
	}

	/* Get info about the received UDP packet */
	wiced_udp_packet_get_info( packet, &udp_src_ip_addr, &udp_src_port );

	/* Extract the received data from the UDP packet */
	wiced_packet_get_data( packet, 0, (uint8_t**) &rx_data, &rx_data_length, &available_data_length );

	/* Null terminate the received data, just in case the sender didn't do this */
	//rx_data[ rx_data_length ] = '\x0';

	WPRINT_APP_INFO ( ("UDP Rx: \"%.2x %.2x %.2x\" from %s IP %u.%u.%u.%u:%d\n", rx_data[0], rx_data[1], rx_data[2], (socket == &next_socket)?"AP":"STA",
																  (unsigned char) ( ( GET_IPV4_ADDRESS(udp_src_ip_addr) >> 24 ) & 0xff ),
																  (unsigned char) ( ( GET_IPV4_ADDRESS(udp_src_ip_addr) >> 16 ) & 0xff ),
																  (unsigned char) ( ( GET_IPV4_ADDRESS(udp_src_ip_addr) >>	8 ) & 0xff ),
																  (unsigned char) ( ( GET_IPV4_ADDRESS(udp_src_ip_addr) >>	0 ) & 0xff ),
																  udp_src_port ) );
	msgp = (msg_t *)rx_data;
	//WPRINT_APP_INFO(("msg->msg_type = %x \n", msg->msg_type));
	if(msg_type_cmp(msgp->msg_type, msg_handshake_syn)) {
		handshake(socket, &udp_src_ip_addr, msg_handshake_ack);
	} else if(msg_type_cmp(msgp->msg_type, msg_bst_control)) {
		if(mac_addr_cmp(msgp->obj_id.mac_addr, this_dev.mac_addr) || mac_addr_cmp(msgp->obj_id.mac_addr, broadcast_mac)) {
			slave_parse_socket_msg(&udp_src_ip_addr, udp_src_port, rx_data, rx_data_length);
		}
		if(this_dev.net_mode == NET_MODE_CHAIN && !mac_addr_cmp(msgp->obj_id.mac_addr, this_dev.mac_addr)) {
			send_to_next_dev(rx_data, rx_data_length);
		}
	}
	/* Delete the received packet, it is no longer needed */
	wiced_packet_delete( packet );

	return WICED_SUCCESS;
}

static wiced_result_t next_receive_callback(void *socket)
{
	wiced_packet_t* 		  packet;
	char*					  rx_data;
	static uint16_t 		  rx_data_length;
	uint16_t				  available_data_length;
	static wiced_ip_address_t udp_src_ip_addr;
	static uint16_t 		  udp_src_port;
	msg_t *msgp;

	/* Wait for UDP packet */
	wiced_result_t result = wiced_udp_receive( socket, &packet, RX_WAIT_TIMEOUT );

	if ( ( result == WICED_ERROR ) || ( result == WICED_TIMEOUT ) )
	{
		return result;
	}

	/* Get info about the received UDP packet */
	wiced_udp_packet_get_info( packet, &udp_src_ip_addr, &udp_src_port );

	/* Extract the received data from the UDP packet */
	wiced_packet_get_data( packet, 0, (uint8_t**) &rx_data, &rx_data_length, &available_data_length );

	/* Null terminate the received data, just in case the sender didn't do this */
	//rx_data[ rx_data_length ] = '\x0';

	WPRINT_APP_INFO ( ("UDP Rx: \"%.2x %.2x %.2x\" from %s IP %u.%u.%u.%u:%d\n", rx_data[0], rx_data[1], rx_data[2], (socket == &next_socket)?"AP":"STA",
																  (unsigned char) ( ( GET_IPV4_ADDRESS(udp_src_ip_addr) >> 24 ) & 0xff ),
																  (unsigned char) ( ( GET_IPV4_ADDRESS(udp_src_ip_addr) >> 16 ) & 0xff ),
																  (unsigned char) ( ( GET_IPV4_ADDRESS(udp_src_ip_addr) >>	8 ) & 0xff ),
																  (unsigned char) ( ( GET_IPV4_ADDRESS(udp_src_ip_addr) >>	0 ) & 0xff ),
																  udp_src_port ) );
	msgp = (msg_t *)rx_data;
	//WPRINT_APP_INFO(("msg->msg_type = %x \n", msg->msg_type));
	if(msg_type_cmp(msgp->msg_type, msg_handshake_syn)) {
		if(this_dev.next_dev_ip == NULL) {
			this_dev.next_dev_ip = malloc(sizeof(wiced_ip_address_t));
			memset(this_dev.next_dev_ip, 0, sizeof(wiced_ip_address_t));
			//add for test 2015.08.29
			wiced_rtos_register_timed_event( &test_event, WICED_NETWORKING_WORKER_THREAD, &test_event_fun, 20*SECONDS, 0 );
		}
		*(this_dev.next_dev_ip) = udp_src_ip_addr;
		handshake(socket, &udp_src_ip_addr, msg_handshake_ack);
	} else if(msg_type_cmp(msgp->msg_type, msg_handshake_ack)) {
		if(this_dev.next_dev_ip == NULL) {
			this_dev.next_dev_ip = malloc(sizeof(wiced_ip_address_t));
			memset(this_dev.next_dev_ip, 0, sizeof(wiced_ip_address_t));
			//add for test 2015.08.29
			wiced_rtos_register_timed_event( &test_event, WICED_NETWORKING_WORKER_THREAD, &test_event_fun, 20*SECONDS, 0 );
		}
		*(this_dev.next_dev_ip) = udp_src_ip_addr;
		alive_timeout = 0;
	} else if(msg_type_cmp(msgp->msg_type, msg_bst_control)) {
		if(this_dev.dev_type == DEV_TYPE_MASTER) {
			master_parse_socket_msg(&udp_src_ip_addr, udp_src_port, rx_data, rx_data_length);
		} else {
			send_to_pre_dev(rx_data, rx_data_length);
		}
	}
	/* Delete the received packet, it is no longer needed */
	wiced_packet_delete( packet );

	return WICED_SUCCESS;
}

wiced_result_t master_parse_socket_msg (const wiced_ip_address_t* ip_addr, const uint16_t udp_port, char* buffer, uint16_t buffer_length)
{
	msg_t *msgp;
	wiced_ip_address_t target_addr;

	msgp = (msg_t *)buffer;
	
	WPRINT_APP_INFO(("master_parse_socket_msg\n"));
	if(this_dev.net_mode == NET_MODE_CHAIN) {
		if(msgp->flag == MSG_MOBILE_DEVICE) {
			target_addr.ip.v4 = msgp->ip_addr;
			send_udp_packet(&user_socket, &target_addr, msgp->udp_port, buffer, buffer_length);
		} else {
			WPRINT_APP_INFO(("forward_to_control_panel\n"));
			wiced_uart_transmit_bytes( WICED_UART_2, buffer, buffer_length);
		}
	} else {
		WPRINT_APP_INFO(("forward_to_control_panel\n"));
		msgp->ip_addr = ip_addr->ip.v4;
		wiced_uart_transmit_bytes( WICED_UART_2, buffer, buffer_length);
	}
	return WICED_SUCCESS;
}

wiced_result_t slave_parse_socket_msg(const wiced_ip_address_t* ip_addr, const uint16_t udp_port, char *rx_data, uint16_t rx_data_length)
{
	msg_t *msgp;
	int i;
	
	msgp = (msg_t *)rx_data;

	if(msgp->obj_id.obj_type == OBJ_TYPE_LIGHT || msgp->obj_id.obj_type == 0xff) {
		if(this_dev.light_dev != NULL) {
			for(i = 0; i < this_dev.light_dev->light_count; i++) {
				if(msgp->obj_id.obj_no == this_dev.light_dev->light_list[i].light_no) {
					light_parse_msg(ip_addr, udp_port, &this_dev.light_dev->light_list[i], msgp);
					break;
				}else if(msgp->obj_id.obj_no == 0xff) {
					light_parse_msg(ip_addr, udp_port, &this_dev.light_dev->light_list[i], msgp);
				}
			}
		}
	} else if(msgp->obj_id.obj_type == OBJ_TYPE_CURTAIN || msgp->obj_id.obj_type == 0xff) {
		if(this_dev.curtain_dev != NULL) {
			for(i = 0; i < this_dev.curtain_dev->curtain_count; i++) {
				if(msgp->obj_id.obj_no == this_dev.curtain_dev->curtain_list[i].curtain_no) {
					curtain_parse_msg(ip_addr, udp_port, &this_dev.curtain_dev->curtain_list[i], msgp);
					break;			
				}else if(msgp->obj_id.obj_no == 0xff) {
					curtain_parse_msg(ip_addr, udp_port, &this_dev.curtain_dev->curtain_list[i], msgp);
				}
			}
		}
	}
	return WICED_SUCCESS;
}

static wiced_result_t light_parse_msg(const wiced_ip_address_t* ip_addr, const uint16_t udp_port, light_t *light, msg_t *msgp)
{
	msg_t msg;
	
	memset(&msg, 0, sizeof(msg_t));
	
	memcpy(msg.msg_type, msg_bst_control, sizeof(msg.msg_type));
	msg.nonce = msgp->nonce;
	msg.fun_type = msgp->fun_type;
	msg.flag = msgp->flag;
	msg.ip_addr = msgp->ip_addr;
	msg.udp_port = msgp->udp_port;
	
	msg.obj_id.mac_addr = this_dev.mac_addr;
	msg.obj_id.obj_type = OBJ_TYPE_LIGHT;
	msg.obj_id.obj_no = light->light_no;
	
	switch(msgp->fun_type) {
		case DEVICE_GET_INFO:
			msg.byte28 = OBJ_TYPE_LIGHT;
			strncpy(&msg.byte29, light->light_name, sizeof(light->light_name));
			msg.data_len = 33;
			//WPRINT_APP_INFO(("msg.byte21 is %s\n", &msg.byte21));
			send_response(ip_addr, udp_port, &msg, MSG_HEAD_LEN + msg.data_len);
			break;

		case LIGHT_FUN_SET_STATE:
			WPRINT_APP_INFO(("LIGHT_FUN_SET_STATE\n"));
			set_light_status(light, msgp->fun_data);
			msg.data_len = 0;
			send_response(ip_addr, udp_port, &msg, MSG_HEAD_LEN + msg.data_len);
			break;

		case LIGHT_FUN_GET_STATE:
			WPRINT_APP_INFO(("LIGHT_FUN_GET_STATE\n"));
			msg.fun_data = get_light_status(light);
			msg.data_len = 0;
			send_response(ip_addr, udp_port, &msg, MSG_HEAD_LEN + msg.data_len);
			break;

		default:
			msg.fun_data = -1;
			msg.data_len = 0;
			if(mac_addr_cmp(msgp->obj_id.mac_addr, this_dev.mac_addr) && \
				msgp->obj_id.obj_type == OBJ_TYPE_LIGHT && \
				msgp->obj_id.obj_no == light->light_no) {
				send_response(ip_addr, udp_port, &msg, MSG_HEAD_LEN + msg.data_len);
			}
			break;
	}
	return WICED_SUCCESS;
}

static wiced_result_t curtain_parse_msg(const wiced_ip_address_t* ip_addr, const uint16_t udp_port, curtain_t *curtain, msg_t *msgp)
{
	msg_t msg;
	
	memset(&msg, 0, sizeof(msg_t));
	
	memcpy(msg.msg_type, msg_bst_control, sizeof(msg.msg_type));
	msg.nonce = msgp->nonce;
	msg.fun_type = msgp->fun_type;
	msg.flag = msgp->flag;
	msg.ip_addr = msgp->ip_addr;
	msg.udp_port = msgp->udp_port;
	
	msg.obj_id.mac_addr = this_dev.mac_addr;
	msg.obj_id.obj_type = OBJ_TYPE_CURTAIN;
	msg.obj_id.obj_no = curtain->curtain_no;

	switch(msgp->fun_type) {
		case DEVICE_GET_INFO:
			msg.byte28 = OBJ_TYPE_CURTAIN;
			strncpy(&msg.byte29, curtain->curtain_name, sizeof(curtain->curtain_name));
			msg.data_len = 33;
			//WPRINT_APP_INFO(("msg.byte21 is %s\n", &msg.byte21));
			send_response(ip_addr, udp_port, &msg, MSG_HEAD_LEN + msg.data_len);
			break;

		case CURTAIN_FUN_SET_POS:
			WPRINT_APP_INFO(("CURTAIN_FUN_SET_POS\n"));
			msg.fun_data= set_curtain_pos(curtain, msgp->fun_data);
			msg.data_len = 0;
			send_response(ip_addr, udp_port, &msg, MSG_HEAD_LEN + msg.data_len);
			break;

		case CURTAIN_FUN_GET_POS:
			WPRINT_APP_INFO(("CURTAIN_FUN_GET_POS\n"));
			msg.fun_data = get_curtain_pos(curtain);
			msg.data_len = 0;
			send_response(ip_addr, udp_port, &msg, MSG_HEAD_LEN + msg.data_len);
			break;

		default:
			msg.fun_data = -1;
			msg.data_len = 0;
			if(mac_addr_cmp(msgp->obj_id.mac_addr, this_dev.mac_addr) && \
				msgp->obj_id.obj_type == OBJ_TYPE_CURTAIN&& \
				msgp->obj_id.obj_no == curtain->curtain_no) {
				send_response(ip_addr, udp_port, &msg, MSG_HEAD_LEN + msg.data_len);
			}
			break;
	}
	return WICED_SUCCESS;
}

static wiced_result_t sta_conn_check(void *arg)
{
	wiced_ip_address_t gw_ip_addr;
	static int conn_count;
	
	WPRINT_APP_INFO( ("sta_conn_check\n") );
	if(conn_count++ >= 20)
		reboot();
	if(wiced_network_is_up(WICED_STA_INTERFACE) == WICED_TRUE) {
		if(wiced_ip_get_gateway_address(WICED_STA_INTERFACE, &gw_ip_addr) != WICED_SUCCESS) {
			WPRINT_APP_INFO( ("get geteway address failed\n") );
			return WICED_ERROR;
		}
		WPRINT_APP_INFO ( ("geteway ip address:  %u.%u.%u.%u\n", (unsigned char) ( ( GET_IPV4_ADDRESS(gw_ip_addr) >> 24 ) & 0xff ),
                                                              (unsigned char) ( ( GET_IPV4_ADDRESS(gw_ip_addr) >> 16 ) & 0xff ),
                                                              (unsigned char) ( ( GET_IPV4_ADDRESS(gw_ip_addr) >>  8 ) & 0xff ),
                                                              (unsigned char) ( ( GET_IPV4_ADDRESS(gw_ip_addr) >>  0 ) & 0xff )
                                                               ) );
		
		wiced_rtos_deregister_timed_event(&sta_conn_check_event);

		/* Create UDP socket */
	    if ( wiced_udp_create_socket( &pre_socket, PORTNUM, WICED_STA_INTERFACE ) != WICED_SUCCESS )
	    {
	        WPRINT_APP_INFO( ("UDP socket creation failed\n") );
			return WICED_ERROR;
	    }
		
		//wiced_rtos_create_thread(&sta_receive_thread, WICED_NETWORK_WORKER_PRIORITY, "sta udp receive thread", udp_sta_thread_main, UDP_RECEIVE_STACK_SIZE, 0);
		if(this_dev.dev_type == DEV_TYPE_SLAVE) {
			if(this_dev.net_mode == NET_MODE_CHAIN) {
				if(this_dev.pre_dev_ip == NULL) {
					this_dev.pre_dev_ip = malloc(sizeof(wiced_ip_address_t));
				}
				*(this_dev.pre_dev_ip) = gw_ip_addr;
				handshake(&pre_socket, this_dev.pre_dev_ip, msg_handshake_syn);
			}
			wiced_udp_register_callbacks(&pre_socket, slave_receive_callback);
		} else if (this_dev.dev_type == DEV_TYPE_MASTER) {
			wiced_udp_register_callbacks(&pre_socket, master_receive_callback);
		}
		WPRINT_APP_INFO(("WICED_STA_INTERFACE wiced_udp_register_callbacks\n"));
	}
	return WICED_SUCCESS;
}

static wiced_result_t sta_reconn_check(void *arg)
{
	wiced_ip_address_t ip_addr;
	wiced_ip_address_t gw_ip_addr;
	static int conn_count;
	
	WPRINT_APP_INFO( ("sta_reconn_check\n") );
	if(conn_count++ >= 60)
		reboot();
	if(reconnected_status == 1) {
		/* Register callbacks */
		//wiced_network_register_link_callback( link_up, link_down );

		/* Initialise semaphore to notify when the network comes up */
		//wiced_rtos_init_semaphore( &link_up_semaphore );
		
		/* The link_up() function sets a semaphore when the link is back up. Wait here until the semaphore is set */
    	//wiced_rtos_get_semaphore( &link_up_semaphore, WICED_NEVER_TIMEOUT );

		//wiced_rtos_delay_milliseconds(1000);

		if(wiced_ip_get_gateway_address(WICED_STA_INTERFACE, &gw_ip_addr) != WICED_SUCCESS) {
			WPRINT_APP_INFO( ("get geteway address failed\n") );
			return WICED_ERROR;
		}
		WPRINT_APP_INFO ( ("geteway ip address:  %u.%u.%u.%u\n", (unsigned char) ( ( GET_IPV4_ADDRESS(gw_ip_addr) >> 24 ) & 0xff ),
                                                              (unsigned char) ( ( GET_IPV4_ADDRESS(gw_ip_addr) >> 16 ) & 0xff ),
                                                              (unsigned char) ( ( GET_IPV4_ADDRESS(gw_ip_addr) >>  8 ) & 0xff ),
                                                              (unsigned char) ( ( GET_IPV4_ADDRESS(gw_ip_addr) >>  0 ) & 0xff )
                                                               ) );
		wiced_ip_get_ipv4_address( WICED_STA_INTERFACE, &ip_addr );
		if(GET_IPV4_ADDRESS(ip_addr) != 0) {
			wiced_rtos_deregister_timed_event(&sta_conn_check_event);

			if(this_dev.pre_dev_ip == NULL) {
				this_dev.pre_dev_ip = malloc(sizeof(wiced_ip_address_t));
			}
			*(this_dev.pre_dev_ip) = gw_ip_addr;

			handshake(&pre_socket, this_dev.pre_dev_ip, msg_handshake_syn);
		}
		
		//wiced_rtos_register_timed_event( &process_sta_rx_event, WICED_NETWORKING_WORKER_THREAD, &process_pre_dev_packet, 1*SECONDS, 0 );
		//wiced_rtos_create_thread(&sta_receive_thread, WICED_NETWORK_WORKER_PRIORITY, "sta udp receive thread", udp_sta_thread_main, UDP_RECEIVE_STACK_SIZE, 0);
		//wiced_udp_register_callbacks(&sta_socket, socket_receive_callback);
	}
	return WICED_SUCCESS;
}

static wiced_result_t test_event_fun(void *arg)
{

	//handshake(&ap_socket, &udp_broadcast_addr, msg_handshake_syn);
	handshake(&next_socket, this_dev.next_dev_ip, msg_handshake_syn);

	if(alive_timeout++ >= 3)
		reboot();

	return WICED_SUCCESS;
}

wiced_result_t send_response(const wiced_ip_address_t* ip_addr, const uint16_t udp_port, char *buffer, uint16_t length)
{
	if(this_dev.net_mode == NET_MODE_CHAIN) {
		send_to_pre_dev(buffer, length);
	} else {
		send_udp_packet(&pre_socket, ip_addr, udp_port, buffer, length);
	}
	return WICED_SUCCESS;
}

wiced_result_t send_to_target_dev(char *buffer, uint16_t length)
{
	msg_t *msgp;
	wiced_ip_address_t ip_addr;
	wiced_ip_address_t gw_ip_addr;
	wiced_ip_address_t netmask;

	msgp = (msg_t *)buffer;

	if(wiced_ip_get_gateway_address(WICED_STA_INTERFACE, &gw_ip_addr) != WICED_SUCCESS) {
		WPRINT_APP_INFO( ("get geteway address failed\n") );
		return WICED_ERROR;
	}

	wiced_ip_get_netmask(WICED_STA_INTERFACE, &netmask);

	ip_addr.ip.v4 = (netmask.ip.v4 & gw_ip_addr.ip.v4) | (~netmask.ip.v4 & msgp->ip_addr);
	
	send_udp_packet(&pre_socket, &ip_addr, PORTNUM, buffer, length);
	return WICED_SUCCESS;
}

wiced_result_t send_to_pre_dev(char *buffer, uint16_t length)
{
	send_udp_packet(&pre_socket, this_dev.pre_dev_ip, PORTNUM, buffer, length);
	return WICED_SUCCESS;
}

wiced_result_t send_to_next_dev(char *buffer, uint16_t length)
{
	send_udp_packet(&next_socket, this_dev.next_dev_ip, PORTNUM, buffer, length);
	return WICED_SUCCESS;
}

wiced_result_t send_udp_packet(wiced_udp_socket_t* socket, const wiced_ip_address_t* ip_addr, const uint16_t udp_port, char *buffer, uint16_t length)
{
	wiced_packet_t* 		 packet;
	char*					 udp_data;
	uint16_t				 available_data_length;

	if(ip_addr == NULL || GET_IPV4_ADDRESS(*ip_addr) == 0) {
		WPRINT_APP_INFO( ("dest ip addr is NULL, start device finding and try again\n") );
		handshake(socket, &udp_broadcast_addr, msg_handshake_syn);
		return WICED_ERROR;
	}
	
	/* Create the UDP packet */
	if ( wiced_packet_create_udp( socket, UDP_MAX_DATA_LENGTH, &packet, (uint8_t**) &udp_data, &available_data_length ) != WICED_SUCCESS )
	{
		WPRINT_APP_INFO( ("UDP tx packet creation failed\n") );
		return WICED_ERROR;
	}

	/* Write packet number into the UDP packet data */
	memcpy(udp_data, buffer, length);

	/* Set the end of the data portion */
	wiced_packet_set_data_end( packet, (uint8_t*) udp_data + length );

	/* Send the UDP packet */
	if ( wiced_udp_send( socket, ip_addr, udp_port, packet ) != WICED_SUCCESS )
	{
		WPRINT_APP_INFO( ("UDP packet send failed\n") );
		wiced_packet_delete( packet ); /* Delete packet, since the send failed */
		return WICED_ERROR;
	}

	WPRINT_APP_INFO ( ("send_udp_packet target ip_addr:  %u.%u.%u.%u via %s\n",  (unsigned char) ( ( GET_IPV4_ADDRESS(*ip_addr) >> 24 ) & 0xff ),
													  (unsigned char) ( ( GET_IPV4_ADDRESS(*ip_addr) >> 16 ) & 0xff ),
													  (unsigned char) ( ( GET_IPV4_ADDRESS(*ip_addr) >>  8 ) & 0xff ),
													  (unsigned char) ( ( GET_IPV4_ADDRESS(*ip_addr) >>  0 ) & 0xff ),
													   (socket == &next_socket)?"AP":"STA") );
	return WICED_SUCCESS;
}

static wiced_result_t handshake(wiced_udp_socket_t* socket, wiced_ip_address_t *ip_addr, const msg_type_t msg_type)
{
	//wiced_udp_socket_t *udp_socket;
	msg_t msg;

	memset(&msg, 0, sizeof(msg));
	memcpy(msg.msg_type, msg_type, 3);

	if(send_udp_packet(socket, ip_addr, PORTNUM, (char *)&msg, sizeof(msg)) != WICED_SUCCESS) {
		return WICED_ERROR;
	}
    return WICED_SUCCESS;
}

static wiced_bool_t msg_type_cmp(const msg_type_t msg_type1, const msg_type_t msg_type2)
{
	int i;
	char *p = (char *)msg_type1;
	char *q = (char *)msg_type2;
	for(i=0; i<3; i++){
		if(*p++ != *q++)
			return WICED_FALSE;
		}
	return WICED_TRUE;
}

static wiced_bool_t mac_addr_cmp(wiced_mac_t addr1, wiced_mac_t addr2)
{
	int i;

	for(i = 0; i < sizeof(addr1.octet); i++) {
		if(addr1.octet[i] != addr2.octet[i]) {
			return WICED_FALSE;
		}
	}
	return WICED_TRUE;
}

static void link_up( void )
{

	WPRINT_APP_INFO( ("And we're connected again!\n") );
    /* Set a semaphore to indicate the link is back up */
    //wiced_rtos_set_semaphore( &link_up_semaphore );

	reconnected_status = 1;
}

static void link_down( void )
{
    WPRINT_APP_INFO( ("Network connection is down.\n") );

	reconnected_status = 0;

	wiced_rtos_register_timed_event( &sta_conn_check_event, WICED_NETWORKING_WORKER_THREAD, &sta_reconn_check, 1*SECONDS, 0 );
	
	//wiced_udp_delete_socket(&sta_socket);
}
