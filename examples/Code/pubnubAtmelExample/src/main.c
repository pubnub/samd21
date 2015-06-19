/**
 *
 * \file
 *
 * \brief WINC1500 TCP Client Example.
 *
 * Copyright (c) 2014 Atmel Corporation. All rights reserved.
 *
 * \asf_license_start
 *
 * \page License
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. The name of Atmel may not be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * 4. This software may only be redistributed and used in connection with an
 *    Atmel microcontroller product.
 *
 * THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * EXPRESSLY AND SPECIFICALLY DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * \asf_license_stop
 *
 */

#include "asf.h"
#include "conf_uart_serial.h"
#include "common/include/nm_common.h"
#include "driver/include/m2m_wifi.h"
#include "socket/include/socket.h"
#include "conf_button_led.h"
#include "main.h"
#include "stdio.h"
#include "PubNub.h"
#include "stdlib.h"


typedef enum {
	PUBNUB_WIFI_MODE_UNKNOWN,
	PUBNUB_WIFI_MODE_PROVISION,		
	PUBNUB_WIFI_MODE_STA_ENABLE,
	PUBNUB_WIFI_MODE_STA_TRY_COMPLETE,
	PUBNUB_WIFI_MODE_STA_COMPLETED,
}PubNubWifiMode;

PubNubWifiMode gCurrentWifiMode = PUBNUB_WIFI_MODE_UNKNOWN;

#define STRING_EOL    "\r\n"
#define STRING_HEADER "-- WINC1500 PubNub example --"STRING_EOL	\
	"-- "BOARD_NAME " --"STRING_EOL	\
	"-- Compiled: "__DATE__ " "__TIME__ " --"STRING_EOL

#define MAIN_M2M_AP_SEC                  M2M_WIFI_SEC_OPEN
#define MAIN_M2M_AP_WEP_KEY              "1234567890"
#define MAIN_M2M_AP_SSID_MODE            SSID_MODE_VISIBLE

#define MAIN_HTTP_PROV_SERVER_DOMAIN_NAME    "atmelconfig.com"
#define MAIN_HTTP_PROV_SERVER_IP_ADDRESS     {192, 168, 1, 1}

#define MAIN_M2M_DEVICE_NAME                 "ATMEL_PubNub_00:00"
#define MAIN_MAC_ADDRESS                     {0xf8, 0xf0, 0x05, 0x45, 0xD4, 0x84}
		
#define MAX_LIGHT_VALUE 40
#define LIGHT_SENSOR_RESOLUTION	4096 //ADC_RESOLUTION_12BIT


static tstrM2MAPConfig gstrM2MAPConfig = {
	MAIN_M2M_DEVICE_NAME,
	1,
	0,
	WEP_40_KEY_STRING_SIZE,
	MAIN_M2M_AP_WEP_KEY,
	(uint8)MAIN_M2M_AP_SEC,
	MAIN_M2M_AP_SSID_MODE
};

static CONST uint8 gau8HttpProvServerIP[] = MAIN_HTTP_PROV_SERVER_IP_ADDRESS;
static CONST char gacHttpProvDomainName[] = MAIN_HTTP_PROV_SERVER_DOMAIN_NAME;

static uint8 gau8MacAddr[] = MAIN_MAC_ADDRESS;
static sint8 gacDeviceName[] = MAIN_M2M_DEVICE_NAME;

static char gSSID[ 64 ] = { 0, };
static int gAUTH = 2;
static char gPSK[ 64 ] = { 0, };

/** UART module for debug. */
static struct usart_module cdc_uart_module;

/** Using IP address. */
#define IPV4_BYTE(val, index)           ((val >> (index * 8)) & 0xFF)

/** pubnub */
static const char pubkey[] = "pub-c-6dbe7bfd-6408-430a-add4-85cdfe856b47";
static const char subkey[] = "sub-c-2a73818c-d2d3-11e3-9244-02ee2ddab7fe";
static const char channel[] = "Atmel_Pubnub";
//static const char channel[] = "Atmel_Pubnub_SJO";
static const char r21_channel[] = "Atmel_Pubnub";
static pubnub_t *m_pb;


/* Light sensor module. */
struct adc_module adc_instance;


/**
 * \brief Callback to get the Data from socket.
 *
 * \param[in] sock socket handler.
 * \param[in] u8Msg socket event type. Possible values are:
 *  - SOCKET_MSG_BIND
 *  - SOCKET_MSG_LISTEN
 *  - SOCKET_MSG_ACCEPT
 *  - SOCKET_MSG_CONNECT
 *  - SOCKET_MSG_RECV
 *  - SOCKET_MSG_SEND
 *  - SOCKET_MSG_SENDTO
 *  - SOCKET_MSG_RECVFROM
 * \param[in] pvMsg is a pointer to message structure. Existing types are:
 *  - tstrSocketBindMsg
 *  - tstrSocketListenMsg
 *  - tstrSocketAcceptMsg
 *  - tstrSocketConnectMsg
 *  - tstrSocketRecvMsg
 */
static void m2m_tcp_socket_handler(SOCKET sock, uint8_t u8Msg, void *pvMsg)
{
	handle_tcpip(sock, u8Msg, pvMsg);
}

static void socket_resolve_cb(uint8_t *hostName, uint32_t hostIp)
{
	printf("Host IP is %d.%d.%d.%d\r\n", (int)IPV4_BYTE(hostIp, 0), (int)IPV4_BYTE(hostIp, 1), (int)IPV4_BYTE(hostIp, 2), (int)IPV4_BYTE(hostIp, 3));
	printf("Host Name is %s\r\n", hostName);
	
	handle_dns_found((char*)hostName, hostIp);
}

/**
 * \brief Callback to get the Wi-Fi status update.
 *
 * \param[in] u8MsgType type of Wi-Fi notification. Possible types are:
 *  - [M2M_WIFI_RESP_CURRENT_RSSI](@ref M2M_WIFI_RESP_CURRENT_RSSI)
 *  - [M2M_WIFI_RESP_CON_STATE_CHANGED](@ref M2M_WIFI_RESP_CON_STATE_CHANGED)
 *  - [M2M_WIFI_RESP_CONNTION_STATE](@ref M2M_WIFI_RESP_CONNTION_STATE)
 *  - [M2M_WIFI_RESP_SCAN_DONE](@ref M2M_WIFI_RESP_SCAN_DONE)
 *  - [M2M_WIFI_RESP_SCAN_RESULT](@ref M2M_WIFI_RESP_SCAN_RESULT)
 *  - [M2M_WIFI_REQ_WPS](@ref M2M_WIFI_REQ_WPS)
 *  - [M2M_WIFI_RESP_IP_CONFIGURED](@ref M2M_WIFI_RESP_IP_CONFIGURED)
 *  - [M2M_WIFI_RESP_IP_CONFLICT](@ref M2M_WIFI_RESP_IP_CONFLICT)
 *  - [M2M_WIFI_RESP_P2P](@ref M2M_WIFI_RESP_P2P)
 *  - [M2M_WIFI_RESP_AP](@ref M2M_WIFI_RESP_AP)
 *  - [M2M_WIFI_RESP_CLIENT_INFO](@ref M2M_WIFI_RESP_CLIENT_INFO)
 * \param[in] pvMsg A pointer to a buffer containing the notification parameters
 * (if any). It should be casted to the correct data type corresponding to the
 * notification type. Existing types are:
 *  - tstrM2mWifiStateChanged
 *  - tstrM2MWPSInfo
 *  - tstrM2MP2pResp
 *  - tstrM2MAPResp
 *  - tstrM2mScanDone
 *  - tstrM2mWifiscanResult
 */
static void wifi_cb(uint8_t u8MsgType, void *pvMsg)
{
	printf( "wifi_cb / %d\r\n", u8MsgType );
	
	switch (u8MsgType) {
	case M2M_WIFI_RESP_CON_STATE_CHANGED:
	{
		tstrM2mWifiStateChanged *pstrWifiState = (tstrM2mWifiStateChanged *)pvMsg;
		if (pstrWifiState->u8CurrState == M2M_WIFI_CONNECTED) {
			printf("m2m_wifi_state: M2M_WIFI_RESP_CON_STATE_CHANGED: CONNECTED\r\n");
			m2m_wifi_request_dhcp_client();
		}
		else if (pstrWifiState->u8CurrState == M2M_WIFI_DISCONNECTED) {
			printf("m2m_wifi_state: M2M_WIFI_RESP_CON_STATE_CHANGED: DISCONNECTED\r\n");
			gCurrentWifiMode = PUBNUB_WIFI_MODE_STA_TRY_COMPLETE;
			m2m_wifi_connect(gSSID, strlen(gSSID), gAUTH, (char *)gPSK, M2M_WIFI_CH_ALL);
		}
	break;
	}

	case M2M_WIFI_REQ_DHCP_CONF:
	{
		uint8_t *pu8IPAddress = (uint8_t *)pvMsg;
		
		printf("m2m_wifi_state: M2M_WIFI_REQ_DHCP_CONF: IP is %u.%u.%u.%u\r\n",
				pu8IPAddress[0], pu8IPAddress[1], pu8IPAddress[2], pu8IPAddress[3]);

		if( gCurrentWifiMode == PUBNUB_WIFI_MODE_STA_TRY_COMPLETE )
		{
			gCurrentWifiMode = PUBNUB_WIFI_MODE_STA_COMPLETED;
		}
	break;
	}

	case M2M_WIFI_RESP_PROVISION_INFO:
	{
		tstrM2MProvisionInfo *pstrProvInfo = (tstrM2MProvisionInfo *)pvMsg;
		printf("wifi_cb: M2M_WIFI_RESP_PROVISION_INFO.\r\n");
		
		if (pstrProvInfo->u8Status == M2M_SUCCESS) {
			memcpy( gSSID, pstrProvInfo->au8SSID, strlen( (char*)(pstrProvInfo->au8SSID) ) );
			gAUTH = pstrProvInfo->u8SecType;
			memcpy( gPSK, pstrProvInfo->au8Password, strlen( (char*)(pstrProvInfo->au8Password) ) );
			
			printf("wifi_cb: %s, %s, %d\r\n", gSSID, gPSK, gAUTH);

			if( gCurrentWifiMode == PUBNUB_WIFI_MODE_PROVISION )
				gCurrentWifiMode = PUBNUB_WIFI_MODE_STA_ENABLE;
		}
		else {
			printf("wifi_cb: Provision failed.\r\n");
		}
	}
	break;

	case M2M_WIFI_RESP_CONN_INFO: //M2M_WIFI_RESP_CONNTION_STATE:
	{
		tstrM2mWifiStateChanged *pstrInfo = (tstrM2mWifiStateChanged *)pvMsg;
		if (pstrInfo->u8CurrState == M2M_WIFI_DISCONNECTED) {
			printf("wifi_cb: M2M_WIFI_RESP_CONNTION_STATE: Disconnected.\r\n");
		}
		else if (pstrInfo->u8CurrState == M2M_WIFI_CONNECTED) {
			printf("wifi_cb: M2M_WIFI_RESP_CONNTION_STATE: Connected.\r\n");
		}
		else {
			printf("wifi_cb: M2M_WIFI_RESP_CONNTION_STATE: Undefined.\r\n");
		}
	}
	break;

	default:
		{
		break;
		}
	}
}

/**
 * \brief Configure UART console.
 */
static void configure_console(void)
{
	struct usart_config usart_conf;

	usart_get_config_defaults(&usart_conf);
	usart_conf.mux_setting = CONF_STDIO_MUX_SETTING;
	usart_conf.pinmux_pad0 = CONF_STDIO_PINMUX_PAD0;
	usart_conf.pinmux_pad1 = CONF_STDIO_PINMUX_PAD1;
	usart_conf.pinmux_pad2 = CONF_STDIO_PINMUX_PAD2;
	usart_conf.pinmux_pad3 = CONF_STDIO_PINMUX_PAD3;
	usart_conf.baudrate    = CONF_STDIO_BAUDRATE;

	stdio_serial_init(&cdc_uart_module, CONF_STDIO_USART_MODULE, &usart_conf);
	usart_enable(&cdc_uart_module);
}

#define HEX2ASCII(x) (((x) >= 10) ? (((x) - 10) + 'A') : ((x) + '0'))
static void set_dev_name_to_mac(uint8 *name, uint8 *mac_addr)
{
	/* Name must be in the format WINC1500_00:00 */
	uint16 len;

	len = m2m_strlen(name);
	if (len >= 5) {
		name[len - 1] = HEX2ASCII((mac_addr[5] >> 0) & 0x0f);
		name[len - 2] = HEX2ASCII((mac_addr[5] >> 4) & 0x0f);
		name[len - 4] = HEX2ASCII((mac_addr[4] >> 0) & 0x0f);
		name[len - 5] = HEX2ASCII((mac_addr[4] >> 4) & 0x0f);
	}
}

static inline void ssd1306_clear(void)
{
	uint8_t page = 0;
	uint8_t col = 0;

	for (page = 0; page < 4; ++page)
	{
		ssd1306_set_page_address(page);
		ssd1306_set_column_address(0);
		for (col = 0; col < 128; ++col)
		{
			ssd1306_write_data(0x00);
		}
	}
}

static void display_temperature( uint8_t temperatureValue )
{
	char temperatureBuffer[ 128 ] = { 0, };
	sprintf( temperatureBuffer, "WINC Temperature : %d'F", (temperatureValue*(9/5)+32) );
	
	ssd1306_set_page_address(0);
	ssd1306_set_column_address(0);

	for(int col = 0; col < 128; ++col)
	{
		ssd1306_write_data(0x00);
	}
	
	ssd1306_write_text( temperatureBuffer );
}

static void display_light( uint8_t lightValue )
{
	char lightBuffer[ 128 ] = { 0, };
	sprintf( lightBuffer, "WINC Light : %d", lightValue );
	
	ssd1306_set_page_address(1);
	ssd1306_set_column_address(0);
	
	for(int col = 0; col < 128; ++col)
	{
		ssd1306_write_data(0x00);
	}
	
	ssd1306_write_text( lightBuffer );
}

static void display_received_data( char* receivedData )
{
	ssd1306_set_page_address(2); //2
	ssd1306_set_column_address(0);
	ssd1306_write_text( "SAMR21 Data:" );
		
	ssd1306_set_page_address(3); //3
	ssd1306_set_column_address(0);

	for(int col = 0; col < 128; ++col)
	{
		ssd1306_write_data(0x00);
	}
	
	ssd1306_write_text( receivedData );	
}

static void configure_button_led(void)
{
	struct port_config pin_conf;
	port_get_config_defaults(&pin_conf);

	pin_conf.direction  = PORT_PIN_DIR_OUTPUT;
	port_pin_set_config(EXT3_LED1, &pin_conf);
	port_pin_set_output_level(EXT3_LED1, EXT3_LED_INACTIVE);
	
	/* Set buttons as inputs */
	pin_conf.direction  = PORT_PIN_DIR_INPUT;
	pin_conf.input_pull = PORT_PIN_PULL_UP;
	port_pin_set_config(EXT3_BTN1, &pin_conf);
}

static uint16_t get_light_value(void)
{
	uint16_t light_val;
	status_code_genare_t adc_status;
	adc_start_conversion(&adc_instance);
	do {
		adc_status = adc_read(&adc_instance, &light_val);
	} while (adc_status == STATUS_BUSY);
	return light_val;
}

static void configure_adc(void)
{
	struct adc_config config_adc;
	adc_get_config_defaults(&config_adc);
	config_adc.gain_factor     = ADC_GAIN_FACTOR_DIV2;
	config_adc.clock_prescaler = ADC_CLOCK_PRESCALER_DIV32;
	config_adc.reference       = ADC_REFERENCE_INTVCC1;
	config_adc.positive_input  = ADC_POSITIVE_INPUT_PIN18;
	config_adc.resolution      = ADC_RESOLUTION_12BIT;
	config_adc.freerunning     = true;
	config_adc.left_adjust     = false;
	adc_init(&adc_instance, ADC, &config_adc);
	adc_enable(&adc_instance);
}


/**
 * \brief Main application function.
 *
 * Initialize system, UART console, network then test function of TCP client.
 *
 * \return program return value.
 */
int main(void)
{

	tstrWifiInitParam param;
	int8_t ret;

	uint8 mac_addr[6];
	uint8 u8IsMacAddrValid;

	/* Initialize the board. */
	system_init();

	/* Initialize the UART console. */
	configure_console();
	printf(STRING_HEADER);

	/* Initialize Temperature Sensor */
	at30tse_init();

	ssd1306_init();
	ssd1306_clear();
	/* Initialize the Button/LED. */
	configure_button_led();

	/* Configure the ADC for the light sensor. */
	configure_adc();

	/* Initialize the BSP. */
	nm_bsp_init();

	/* Initialize Wi-Fi parameters structure. */
	memset((uint8_t *)&param, 0, sizeof(tstrWifiInitParam));
	param.pfAppWifiCb = wifi_cb;

	/* Initialize WINC1500 Wi-Fi driver with data and status callbacks. */
	ret = m2m_wifi_init(&param);
	if (M2M_SUCCESS != ret) {
		printf("main: m2m_wifi_init call error!\r\n");
		while (1) {
		}
	}

#ifdef TEST_MODE_WITHOUT_PROVISION
	printf( "TEST_MODE_WITHOUT_PROVISION / Start ~~~\r\n" );
	gCurrentWifiMode = PUBNUB_WIFI_MODE_STA_ENABLE;
#else
	m2m_wifi_get_otp_mac_address(mac_addr, &u8IsMacAddrValid);
	if (!u8IsMacAddrValid) {
		m2m_wifi_set_mac_address(gau8MacAddr);
	}

	m2m_wifi_get_mac_address(gau8MacAddr);

	set_dev_name_to_mac((uint8 *)gacDeviceName, gau8MacAddr);
	set_dev_name_to_mac((uint8 *)gstrM2MAPConfig.au8SSID, gau8MacAddr);
	m2m_wifi_set_device_name((uint8 *)gacDeviceName, (uint8)m2m_strlen((uint8 *)gacDeviceName));

	gCurrentWifiMode = PUBNUB_WIFI_MODE_PROVISION;
	m2m_wifi_start_provision_mode((tstrM2MAPConfig *)&gstrM2MAPConfig, (char *)gacHttpProvDomainName, 1);
	printf("Provision Mode started.\r\nConnect to [%s] via AP[%s] and fill up the page.\r\n", MAIN_HTTP_PROV_SERVER_DOMAIN_NAME, gstrM2MAPConfig.au8SSID);
#endif

	while (1) {
		m2m_wifi_handle_events(NULL);

		if( gCurrentWifiMode == PUBNUB_WIFI_MODE_STA_ENABLE )
		{
			socketInit();
			registerSocketCallback(m2m_tcp_socket_handler, socket_resolve_cb);

#ifdef TEST_MODE_WITHOUT_PROVISION
			memcpy( gSSID, TEST_MODE_SSID, strlen( TEST_MODE_SSID ) );
			gAUTH = 2;
			memcpy( gPSK, TEST_MODE_PASSWORD, strlen( TEST_MODE_PASSWORD ) );
#endif

			/* Connect to router. */
			printf( "gSSID %s, gAUTH %d, gPSK %s\r\n", gSSID, gAUTH, gPSK );
			m2m_wifi_connect( gSSID, strlen(gSSID), gAUTH, gPSK, M2M_WIFI_CH_ALL );

			m_pb = pubnub_get_ctx(0);
			pubnub_init(m_pb, pubkey, subkey);

			gCurrentWifiMode = PUBNUB_WIFI_MODE_STA_TRY_COMPLETE;
		}
		else if( gCurrentWifiMode == PUBNUB_WIFI_MODE_STA_TRY_COMPLETE )
		{

		}
		else if( gCurrentWifiMode == PUBNUB_WIFI_MODE_STA_COMPLETED )
		{
			if(m_pb->state == PS_IDLE) {
				char buf[256] = {0, };
					
				nm_bsp_sleep(800);
				
				if(m_pb->trans == PBTT_NONE || (m_pb->trans == PBTT_SUBSCRIBE && m_pb->last_result == PNR_OK)
					|| (m_pb->trans == PBTT_PUBLISH && m_pb->last_result == PNR_IO_ERROR)) {
					for(;;) {
						char const *msg = pubnub_get(m_pb);
						if(NULL == msg) {
							break;
						} 
						printf("pubnubDemo: Received message: %s\r\n", msg);

						//if( 0 == ( strncmp( &msg[2], "temp", strlen( "temperature" ) ) ) )
							display_received_data( (char*)msg );
					}					
					
					printf("pubnubDemo: publish event.\r\n");
					
					double temperature = at30tse_read_temperature();
					double tempf = (temperature*(9/5))+32;
					uint8_t light = MAX_LIGHT_VALUE - (get_light_value() * MAX_LIGHT_VALUE / LIGHT_SENSOR_RESOLUTION);
					//sprintf(buf, "{\"temperature\":\"%d.%d\", \"light\":\"%d\"}", (int)tempf, (int)((int)(tempf * 100) % 100), light );  
			                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   
					sprintf(buf, "{\"columns\":[[\"temperature\", \"%d.%d\"]]}", (int)tempf, (int)((int)(tempf * 100) % 100));
					display_temperature( (uint8_t)temperature );
					display_light( light );		
					
					pubnub_publish(m_pb, channel, buf);
					/*
					uint8_t button1 = port_pin_get_input_level(EXT3_BTN1);	
					if (0 == button1 )
					{
					  sprintf(buf, "{\"led_status\":\"1\"}");	
					  pubnub_publish(m_pb, channel, buf);	
					}
					*/						
				}
				else {
					printf("pubnubDemo: subscribe event.\r\n");
					
					pubnub_subscribe(m_pb, r21_channel);
				}
			}
		}


	}
 
	return 0;
}
