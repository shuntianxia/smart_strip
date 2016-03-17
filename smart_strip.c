/*
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
 * WICED Configuration Mode Application
 *
 * This application demonstrates how to use WICED Configuration Mode
 * to automatically configure application parameters and Wi-Fi settings
 * via a softAP and webserver
 *
 * Features demonstrated
 *  - WICED Configuration Mode
 *
 * Application Instructions
 *   1. Connect a PC terminal to the serial port of the WICED Eval board,
 *      then build and download the application as described in the WICED
 *      Quick Start Guide
 *   2. After the download completes, the terminal displays WICED startup
 *      information and starts WICED configuration mode.
 *
 * In configuration mode, application and Wi-Fi configuration information
 * is entered via webpages using a Wi-Fi client (eg. your computer)
 *
 * Use your computer to step through device configuration using WICED Config Mode
 *   - Connect the computer using Wi-Fi to the config softAP "WICED Config"
 *     The config AP name & passphrase is defined in the file <WICED-SDK>/include/default_wifi_config_dct.h
 *     The AP name/passphrase is : Wiced Config / 12345678
 *   - Open a web browser and type wiced.com in the URL
 *     (or enter 192.168.0.1 which is the IP address of the softAP interface)
 *   - The Application configuration webpage appears. This page enables
 *     users to enter application specific information such as contact
 *     name and address details for device registration
 *   - Change one of more of the fields in the form and then click 'Save settings'
 *   - Click the Wi-Fi Setup button
 *   - The Wi-Fi configuration page appears. This page provides several options
 *     for configuring the device to connect to a Wi-Fi network.
 *   - Click 'Scan and select network'. The device scans for Wi-Fi networks in
 *     range and provides a webpage with a list.
 *   - Enter the password for your Wi-Fi AP in the Password box (top left)
 *   - Find your Wi-Fi AP in the list, and click the 'Join' button next to it
 *
 * Configuration mode is complete. The device stops the softAP and webserver,
 * and attempts to join the Wi-Fi AP specified during configuration. Once the
 * device completes association, application configuration information is
 * printed to the terminal
 *
 * The wiced.com URL reference in the above text is configured in the DNS
 * redirect server. To change the URL, edit the list in
 * <WICED-SDK>/Library/daemons/dns_redirect.c
 * URLs currently configured are:
 *      # http://www.broadcom.com , http://broadcom.com ,
 *      # http://www.facebook.com , http://facebook.com ,
 *      # http://www.google.com   , http://google.com   ,
 *      # http://www.bing.com     , http://bing.com     ,
 *      # http://www.apple.com    , http://apple.com    ,
 *      # http://www.wiced.com    , http://wiced.com    ,
 *
 *  *** IMPORTANT NOTE ***
 *   The config mode API will be integrated into Wi-Fi Easy Setup when
 *   WICED-SDK-3.0.0 is released.
 *
 */

#include "wiced.h"
#include "smart_strip.h"
#include "smart_strip_dct.h"
#include "power_strip.h"

/******************************************************
 *                      Macros
 ******************************************************/
#define NULL_MODE       0x00
#define STATION_MODE    0x01
#define SOFTAP_MODE     0x02
#define STATIONAP_MODE  0x03

#define RX_WAIT_TIMEOUT        (1*SECONDS)

//#define MAX_RECV_SIZE    8192
#define TCP_PACKET_MAX_DATA_LENGTH        30
#define TCP_CLIENT_INTERVAL               2
#define TCP_CLIENT_CONNECT_TIMEOUT        500
#define TCP_CLIENT_RECEIVE_TIMEOUT        300
#define TCP_CONNECTION_NUMBER_OF_RETRIES  3
//#define RX_BUFFER_SIZE    64

#define UDP_MAX_DATA_LENGTH         256
#define PORTNUM 8088
#define TCP_SERVER_PORT 8000

#define CMD_HEAD_BYTES 5

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
static wiced_result_t dev_init();
static void dev_login_sent(void* socket);
static wiced_result_t tcp_connect_handler(void *arg);
/******************************************************
 *               Variable Definitions
 ******************************************************/
dev_info_t dev_info;

static const wiced_ip_setting_t ap_ip_settings =
{
	INITIALISER_IPV4_ADDRESS( .ip_address, MAKE_IPV4_ADDRESS( 192,168,	1,	1 ) ),
	INITIALISER_IPV4_ADDRESS( .netmask,    MAKE_IPV4_ADDRESS( 255,255,255,	0 ) ),
	INITIALISER_IPV4_ADDRESS( .gateway,    MAKE_IPV4_ADDRESS( 192,168,	1,	1 ) ),
};
/******************************************************
 *               Function Definitions
 ******************************************************/
int reboot()
{
	WPRINT_APP_INFO( ( "Rebooting...\n" ) );
	host_rtos_delay_milliseconds( 1000 );

	wiced_framework_reboot();

	/* Never reached */
	return 0;
}

void print_ip_address(wiced_ip_address_t *ip_addr)
{
	WPRINT_APP_INFO (("%u.%u.%u.%u\n", (unsigned char) ( ( GET_IPV4_ADDRESS(*ip_addr) >> 24 ) & 0xff ),
										(unsigned char) ( ( GET_IPV4_ADDRESS(*ip_addr) >> 16 ) & 0xff ),
										(unsigned char) ( ( GET_IPV4_ADDRESS(*ip_addr) >>  8 ) & 0xff ),
										(unsigned char) ( ( GET_IPV4_ADDRESS(*ip_addr) >>  0 ) & 0xff )
										 ));
}

static int tcp_send_data(wiced_tcp_socket_t* socket, char *sendbuf, int len)
{
	wiced_packet_t* 		 packet;
	char*					 tx_data;
	uint16_t				 available_data_length;
	
	/* Create the TCP packet. Memory for the tx_data is automatically allocated */
	if (wiced_packet_create_tcp(socket, TCP_PACKET_MAX_DATA_LENGTH, &packet, (uint8_t**)&tx_data, &available_data_length) != WICED_SUCCESS)
	{
		WPRINT_APP_INFO(("TCP packet creation failed\n"));
		return WICED_ERROR;
	}

	/* Write the message into tx_data"	*/
	memcpy(tx_data, sendbuf, len);

	/* Set the end of the data portion */
	wiced_packet_set_data_end(packet, (uint8_t*)tx_data + len);

	/* Send the TCP packet */
	if (wiced_tcp_send_packet(socket, packet) != WICED_SUCCESS)
	{
		WPRINT_APP_INFO(("TCP packet send failed\n"));

		/* Delete packet, since the send failed */
		wiced_packet_delete(packet);

		/* Close the connection */
		wiced_tcp_disconnect(socket);
		return WICED_ERROR;
	}

	return WICED_SUCCESS;
}

static int tcp_recv_data(wiced_tcp_socket_t* socket, char *recvbuf, int len)
{
	wiced_packet_t* 		 rx_packet;
	char*					 rx_data;
	uint16_t				 rx_data_length;
	uint16_t				 available_data_length;
	wiced_result_t			 result;

	/* Receive a response from the server */
	result = wiced_tcp_receive(socket, &rx_packet, TCP_CLIENT_RECEIVE_TIMEOUT);
	if( result != WICED_SUCCESS )
	{
		WPRINT_APP_INFO(("TCP packet reception failed\n"));

		/* Delete packet, since the receive failed */
		wiced_packet_delete(rx_packet);

		/* Close the connection */
		wiced_tcp_disconnect(socket);
		return -1;
	}

	/* Get the contents of the received packet */
	wiced_packet_get_data(rx_packet, 0, (uint8_t**)&rx_data, &rx_data_length, &available_data_length);

	/* Null terminate the received string */
	//rx_data[rx_data_length] = '\x0';
	//printf(("rx_data is %s", rx_data));

	memcpy(recvbuf, rx_data, rx_data_length<len?rx_data_length:len);

	printf("rx_data_length = %d\n", rx_data_length);
	//printf("available_data_length = %d\n", available_data_length);
	//printf("recvbuf is %s\n", recvbuf);
	
	/* Delete the packet */
	wiced_packet_delete(rx_packet);
	return rx_data_length;
}

static wiced_result_t send_udp_packet(wiced_udp_socket_t* socket, const wiced_ip_address_t* ip_addr, const uint16_t udp_port, char *buffer, uint16_t length)
{
	wiced_packet_t* 		 packet;
	char*					 udp_data;
	uint16_t				 available_data_length;

	if(ip_addr == NULL || GET_IPV4_ADDRESS(*ip_addr) == 0) {
		WPRINT_APP_INFO( ("dest ip addr is NULL, start device finding and try again\n") );
		//handshake(socket, &udp_broadcast_addr, msg_handshake_syn);
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
	
	return WICED_SUCCESS;
}

void parse_udp_msg(void *socket, const wiced_ip_address_t* ip_addr, const uint16_t udp_port, char *buffer, unsigned short length)
{
    char send_buf[40] = {0};
	uint8_t jack_no;
	jack_status_t jack_status;

    if (buffer == NULL) {
        return;
    }
	char *p = buffer;
	char *q = send_buf;
	unsigned short cmd_head = (*p << 8) | *(p+1);
	printf("cmd_head is %02X%02X\n", *p, *(p+1));
	uint16_t snd_buf_len = 0;
	memcpy(send_buf, buffer, 3);

	if((cmd_head >> 8) == 0xbd || (cmd_head >> 8) == 0xbe)
	{
		switch(cmd_head)
		{
			case 0xbd01:
				*(q+2) = 0xbd;
				*(q+3) = 0xbe;
				memcpy(q+4, dev_info.wifi_dct.mac_address.octet, 6);
				if(dev_info.app_dct.is_set_name == 1) {
					*(q+10) = strlen(dev_info.app_dct.dev_name);
					memcpy(q+11, dev_info.app_dct.dev_name, strlen(dev_info.app_dct.dev_name));
					snd_buf_len = 11 + strlen(dev_info.app_dct.dev_name);
				}
				else {					
					sprintf(q+11, "PT-%02X%02X%02X", dev_info.wifi_dct.mac_address.octet[3], dev_info.wifi_dct.mac_address.octet[4], dev_info.wifi_dct.mac_address.octet[5]);
					*(q+10) = 9;
					snd_buf_len = 11 + *(q+10);
				}
				send_udp_packet(socket, ip_addr, udp_port, send_buf, snd_buf_len);
				break;

			case 0xbd02:
				memset(dev_info.app_dct.dev_name, 0, sizeof(dev_info.app_dct.dev_name));
				memcpy(dev_info.app_dct.dev_name, p+3, *(p+2));
				dev_info.app_dct.is_set_name = 1;
				store_app_data(&dev_info.app_dct);
				*(q+2) = 0;
				snd_buf_len = 3;
				send_udp_packet(socket, ip_addr, udp_port, send_buf, snd_buf_len);
				break;
		
			case 0xbd03:
				*(q+4) = 0;
				snd_buf_len = CMD_HEAD_BYTES + *(q+4);
				send_udp_packet(socket, ip_addr, udp_port, send_buf, snd_buf_len);
				if (*(p+5) == SOFTAP_MODE) {
					if(wiced_network_is_up(WICED_AP_INTERFACE) != WICED_TRUE) {
						printf("change to SOFTAP mode\n");
						dev_info.app_dct.configured = WICED_FALSE;
						store_app_data(&dev_info.app_dct);
						reboot();
					}
				}
				else if(*(p+5) == STATION_MODE)
				{
					if(wiced_network_is_up(WICED_STA_INTERFACE) != WICED_TRUE) {
						printf("change to STATION mode\n");
						dev_info.app_dct.configured = WICED_TRUE;
						store_app_data(&dev_info.app_dct);
						reboot();
					}
				}
				break;
				
			case 0xbd04:		
				memcpy(dev_info.wifi_dct.stored_ap_list[0].details.SSID.value, p+7, *(p+5));
				dev_info.wifi_dct.stored_ap_list[0].details.SSID.value[(uint8_t)*(p+5)] = 0;
				dev_info.wifi_dct.stored_ap_list[0].details.SSID.length = *(p+5);
				
				memcpy(dev_info.wifi_dct.stored_ap_list[0].security_key, p+7+*(p+5), *(p+6));
				dev_info.wifi_dct.stored_ap_list[0].security_key[(uint8_t)*(p+6)] = 0;
				dev_info.wifi_dct.stored_ap_list[0].security_key_length = *(p+6);
				
				*(q+4) = 0;
				snd_buf_len = CMD_HEAD_BYTES + *(q+4);
				printf("ssid is %s, passwd is %s.\n", dev_info.wifi_dct.stored_ap_list[0].details.SSID.value, dev_info.wifi_dct.stored_ap_list[0].security_key);
				send_udp_packet(socket, ip_addr, udp_port, send_buf, snd_buf_len);
				
				if (dev_info.wifi_dct.stored_ap_list[0].details.SSID.length != 0) {
					printf("change to STATION mode\n");
					dev_info.app_dct.configured = WICED_TRUE;
					store_wifi_data(&dev_info.wifi_dct);
					store_app_data(&dev_info.app_dct);
					reboot();
				}
				break;

			case 0xbe01:
				*(q+4) = 3;
				snd_buf_len = CMD_HEAD_BYTES + *(q+4);
				*(q+5) = 0;
				*(q+6) = get_jack_count();
				*(q+7) = get_all_jack_status();
				send_udp_packet(socket, ip_addr, udp_port, send_buf, snd_buf_len);
				break;

			case 0xbe02:
				jack_no = (*(p+5) & 0x0f) + 1;
				jack_status = (*(p+5) >> 4) & 0x01;
				set_jack_status(jack_no, jack_status);
				store_app_data(&dev_info.app_dct);
				*(q+4) = 1;
				*(q+5) = 0;
				snd_buf_len = CMD_HEAD_BYTES + *(q+4);
				send_udp_packet(socket, ip_addr, udp_port, send_buf, snd_buf_len);
				break;

			case 0xbe05:
				*(q+4) = 38;
				snd_buf_len = CMD_HEAD_BYTES + *(q+4);
				*(q+5) = 0;
				*(q+6) = get_all_jack_status();
				send_udp_packet(socket, ip_addr, udp_port, send_buf, snd_buf_len);
				break;

			default:
				break;
		}
	}
}

static wiced_result_t udp_receive_callback(void *socket)
{
	wiced_packet_t* 		  packet;
	char*					  rx_data;
	static uint16_t 		  rx_data_length;
	uint16_t				  available_data_length;
	static wiced_ip_address_t udp_src_ip_addr;
	static uint16_t 		  udp_src_port;

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

	//print_ip_address(&udp_src_ip_addr);
	
	parse_udp_msg(socket, &udp_src_ip_addr, udp_src_port, rx_data, rx_data_length);
	
	/* Delete the received packet, it is no longer needed */
	wiced_packet_delete( packet );

	return WICED_SUCCESS;
}

static wiced_result_t tcp_sent_beacon(void* socket)
{
    if (dev_info.tcp_conn_state == 1) {
        if (dev_info.app_dct.activeflag == 0) {
            printf("plese check device is activated.\n");
            dev_login_sent(socket);
        } else {
			char pbuf[128] = {0};
			pbuf[0] = 0xb1;
			pbuf[1] = 0x03;

            tcp_send_data(socket, pbuf, 2);
        }
    } else {
        printf("tcp_sent_beacon sent fail!\n");
        //user_esp_platform_discon(pespconn);
    }
    return WICED_SUCCESS;
}

static wiced_result_t tcp_connect_callback( void* socket)
{
	printf("tcp_connect_callback\n");
	
    return WICED_SUCCESS;
}

static wiced_result_t tcp_disconnect_callback( void* socket)
{
	wiced_result_t result;
	
	printf("tcp_disconnect_callback\n");
	dev_info.tcp_conn_state = 0;

	wiced_rtos_deregister_timed_event(&dev_info.beacon_event);

	if((result = wiced_tcp_disconnect(&dev_info.tcp_socket)) != WICED_SUCCESS) {
		WPRINT_APP_INFO(("wiced_tcp_disconnect ERROR. result = %d\n", result));
	}

    if(wiced_network_is_up(WICED_STA_INTERFACE) == WICED_TRUE) {
		wiced_rtos_register_timed_event( &dev_info.tcp_conn_event, WICED_NETWORKING_WORKER_THREAD, &tcp_connect_handler, 1*SECONDS, 0 );
    }
	
    return WICED_SUCCESS;
}

static void dev_login_sent(void* socket)
{
	char pbuf[128] = {0};
	char *p = pbuf;

	printf("dev_login_sent\n");

    if (dev_info.app_dct.activeflag == 0xFF) {
        dev_info.app_dct.activeflag = 0;
    }

    if (dev_info.app_dct.activeflag == 0) {
		*(p + 1) = 0x01;
	} else {
		*(p + 1) = 0x02;
    }
	*(p + 0) = 0xb1;
	*(p + 2) = 0xbd;
	*(p + 3) = 0xbe;
	memcpy(p+4, dev_info.wifi_dct.mac_address.octet, 6);
	tcp_send_data(socket, pbuf, 10);
}

void parse_tcp_msg(void *socket, char *buffer, unsigned short length)
{
	uint8_t jack_no;
	jack_status_t jack_status;
    char send_buf[128] = {0};
	char *p = buffer;
	char *q = send_buf;
	unsigned short cmd_head = (*p << 8) | *(p+1);
	uint16_t snd_buf_len = 0;
	memcpy(send_buf, buffer, 2);

    //os_timer_disarm(&beacon_timer);

	if((cmd_head >> 8) == 0xb1) {
		if (cmd_head == 0xb101) {
			if (*(p + 2) == 0) {
	            printf("device activates successful.\n");
				printf("cmd_head is %2x %2x\n", buffer[0], buffer[1]);

	            dev_info.device_status = DEVICE_ACTIVE_DONE;
				dev_info.app_dct.activeflag = 1;
	            store_app_data(&dev_info.app_dct);
	            dev_login_sent(socket);
	        } else {
	            printf("device activates failed.\n");
	            dev_info.device_status = DEVICE_ACTIVE_FAIL;
	        }
	    } else if (cmd_head == 0xb102) {
	    
	    } else if (cmd_head == 0xb103) {
	    	dev_info.ping_status = 1;
	    } else if (cmd_head == 0xb104) {

        } else if (cmd_head == 0xb105) {

        } else if (cmd_head == 0xb106) {

        } else if (cmd_head == 0xb107) {
        
        }
	} else if((cmd_head >> 8) == 0xbe) {
		if(cmd_head == 0xbe01) {
			*(q+4) = 3;
			snd_buf_len = CMD_HEAD_BYTES + *(q+4);
			*(q+5) = 0;
			*(q+6) = get_jack_count();
			*(q+7) = get_all_jack_status();
			tcp_send_data(socket, send_buf, snd_buf_len);
		} else if(cmd_head == 0xbe02) {
			jack_no = (*(p+5) & 0x0f) + 1;
			jack_status = (*(p+5) >> 4) & 0x01;
			set_jack_status(jack_no, jack_status);
			store_app_data(&dev_info.app_dct);
			*(q+4) = 1;
			*(q+5) = 0;
			snd_buf_len = CMD_HEAD_BYTES + *(q+4);
			tcp_send_data(socket, send_buf, snd_buf_len);
		} else if(cmd_head == 0xbe05) {
			*(q+4) = 38;
			snd_buf_len = CMD_HEAD_BYTES + *(q+4);
			*(q+5) = 0;
			*(q+6) = get_all_jack_status();
			tcp_send_data(socket, send_buf, snd_buf_len);
		}
	}
    memset(send_buf, 0, sizeof(send_buf));
    //os_timer_arm(&beacon_timer, BEACON_TIME, 0);
}

static wiced_result_t tcp_receive_callback(void* socket)
{
    wiced_result_t           result;
    //wiced_packet_t*          packet;
    wiced_packet_t*          rx_packet;
    char*                    rx_data;
    uint16_t                 rx_data_length;
    uint16_t                 available_data_length;

	//printf("tcp_receive_callback\n");

    /* Receive a response from the server and print it out to the serial console */
    result = wiced_tcp_receive(socket, &rx_packet, TCP_CLIENT_RECEIVE_TIMEOUT);
   	if ( ( result == WICED_ERROR ) || ( result == WICED_TIMEOUT ) )
	{
		return result;
	}

    /* Get the contents of the received packet */
    wiced_packet_get_data(rx_packet, 0, (uint8_t**)&rx_data, &rx_data_length, &available_data_length);

	parse_tcp_msg(socket, rx_data, rx_data_length);

    /* Delete the packet */
    wiced_packet_delete(rx_packet);

    return WICED_SUCCESS;
}

static wiced_result_t tcp_connect_handler(void *arg)
{
    wiced_result_t           result;
	int connection_retries;
	
	if(wiced_network_is_up(WICED_STA_INTERFACE) == WICED_TRUE) {
		/* Connect to the remote TCP server, try several times */
		connection_retries = 0;
		do
		{ 
			if(wiced_hostname_lookup("www.yekertech.com", &dev_info.server_ip, 1000) != WICED_SUCCESS) {
				WPRINT_APP_INFO(("hostname look up failed\n"));
				return WICED_ERROR;
			}
			result = wiced_tcp_connect( &dev_info.tcp_socket, &dev_info.server_ip, TCP_SERVER_PORT, TCP_CLIENT_CONNECT_TIMEOUT );
			connection_retries++;
		}
		while( ( result != WICED_SUCCESS ) && ( connection_retries < TCP_CONNECTION_NUMBER_OF_RETRIES ) );
		if( result == WICED_SUCCESS)
		{
			dev_info.tcp_conn_state = 1;
			wiced_rtos_deregister_timed_event(&dev_info.tcp_conn_event);
			dev_login_sent(&dev_info.tcp_socket);
			wiced_rtos_register_timed_event( &dev_info.beacon_event, WICED_NETWORKING_WORKER_THREAD, &tcp_sent_beacon, 50*SECONDS, &dev_info.tcp_socket);
		} else {
			WPRINT_APP_INFO(("Unable to connect to the server, retry a later. result = %d\n", result));
		}
	}
	return WICED_SUCCESS;
}

static void tcp_connection_create()
{
	/* Create a TCP socket */
	if (wiced_tcp_create_socket(&dev_info.tcp_socket, WICED_STA_INTERFACE) != WICED_SUCCESS)
	{
		WPRINT_APP_INFO(("TCP socket creation failed\n"));
		return;
	}
	
	/* Bind to the socket */
	//wiced_tcp_bind( &dev_info.tcp_socket, TCP_SERVER_PORT );
	
	wiced_tcp_register_callbacks(&dev_info.tcp_socket, tcp_connect_callback, tcp_receive_callback, tcp_disconnect_callback);
	wiced_rtos_register_timed_event( &dev_info.tcp_conn_event, WICED_NETWORKING_WORKER_THREAD, &tcp_connect_handler, 1*SECONDS, 0 );
}

static void tcp_connection_delete()
{
	wiced_rtos_deregister_timed_event(&dev_info.beacon_event);

	wiced_rtos_deregister_timed_event(&dev_info.tcp_conn_event);

	wiced_tcp_unregister_callbacks(&dev_info.tcp_socket);

	wiced_tcp_delete_socket(&dev_info.tcp_socket);
}

static void link_up( void )
{

	WPRINT_APP_INFO( ("And we're connected again!\n") );
	
	/* Set a semaphore to indicate the link is back up */
	//wiced_rtos_set_semaphore( &link_up_semaphore );

	tcp_connection_create();
}

static void link_down( void )
{
	WPRINT_APP_INFO( ("Network connection is down.\n") );

	dev_info.tcp_conn_state = 0;

	tcp_connection_delete();
}

static wiced_result_t dev_init()
{
	wiced_result_t res;

	load_app_data(&dev_info.app_dct);

	load_wifi_data(&dev_info.wifi_dct);

	res = power_strip_init(&dev_info.power_strip, &dev_info.app_dct.power_strip_config);
	if(res != WICED_SUCCESS) {
		return WICED_ERROR;
	}

	return WICED_SUCCESS;
}

void application_start( )
{
    /* Initialise the device and WICED framework */
    wiced_init( );

	if(dev_init() != WICED_SUCCESS){
		WPRINT_APP_INFO(("device_init failed\n"));
		return;
	}

	if(dev_info.app_dct.configured != WICED_TRUE) {
		memset(dev_info.wifi_dct.soft_ap_settings.SSID.value, 0, sizeof(dev_info.wifi_dct.soft_ap_settings.SSID.value));
		sprintf((char *)dev_info.wifi_dct.soft_ap_settings.SSID.value, "PT-%02X%02X%02X", \
			dev_info.wifi_dct.mac_address.octet[3], \
			dev_info.wifi_dct.mac_address.octet[4], \
			dev_info.wifi_dct.mac_address.octet[5]);
		dev_info.wifi_dct.soft_ap_settings.security = WICED_SECURITY_OPEN;
		store_wifi_data(&dev_info.wifi_dct);
			
		/* Bring up the softAP interface */
	    wiced_network_up(WICED_AP_INTERFACE, WICED_USE_INTERNAL_DHCP_SERVER, &ap_ip_settings);
		
		/* Create UDP socket */
		if ( wiced_udp_create_socket(&dev_info.udp_socket, PORTNUM, WICED_AP_INTERFACE ) != WICED_SUCCESS )
		{
			WPRINT_APP_INFO( ("User UDP socket creation failed\n") );
		}
		
		/* Register a function to process received UDP packets */
		wiced_udp_register_callbacks(&dev_info.udp_socket, udp_receive_callback);
	} else {
		/* Register callbacks */
		wiced_network_register_link_callback( link_up, link_down );

		dev_info.retry_ms = 500;
		/* Bring up the network interface */
		while(wiced_network_up( WICED_STA_INTERFACE, WICED_USE_EXTERNAL_DHCP_SERVER, NULL ) != WICED_SUCCESS) {
			host_rtos_delay_milliseconds(dev_info.retry_ms);
			dev_info.retry_ms *= 2;
			if(dev_info.retry_ms >= 60*1000) {
				dev_info.retry_ms = 500;
			}
		}
		
		/* Create UDP socket */
	    if ( wiced_udp_create_socket( &dev_info.udp_socket, PORTNUM, WICED_STA_INTERFACE ) != WICED_SUCCESS )
	    {
	        WPRINT_APP_INFO( ("UDP socket creation failed\n") );
			return;
	    }

		wiced_udp_register_callbacks(&dev_info.udp_socket, udp_receive_callback);

		tcp_connection_create();
	}
	WPRINT_APP_INFO(("end ...\n"));
}
