/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2017 Eric S. Pooch
 *
 * Permission is hereby granted for use on ESPRESSIF SYSTEMS ESP32 only, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "esp_system.h"

#include "esp_log.h"

#include "at_pppd.h"

#include "driver/uart.h"

#include "netif/ppp/pppos.h"
#include "netif/ppp/ppp.h"
#include "netif/ppp/pppdebug.h"

#include "netif/ppp/pppapi.h"

#include "lwip/sio.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include "debug/lwip_debug.h"

#include "bridge.h"

#ifndef CONFIG_AT_UART_PORT
#define CONFIG_AT_UART_PORT UART_NUM_1
#endif

#ifdef CONFIG_PPP_SUPPORT

#define BUF_SIZE (1024)
#define MAX_IP_LEN (15)

static esp_at_cmd_struct at_pppd_cmd[] = {
    {"+PPPD", at_testCmdPpp, at_queryCmdPpp, at_setupCmdPpp, at_exeCmdPpp},
    {"DTPPPD", at_testCmdPpp, at_queryCmdPpp, at_setupCmdPpp, at_exeCmdPpp},
    {"DTPPPD;", at_testCmdPpp, at_queryCmdPpp, at_setupCmdPpp, at_exeCmdPpp},
};

#define PPP_IP_DELIM ':'
#define PPP_TEST_CMD '?'

#define PPP_AT_PARAMS "\"<local_IP_address>:<remote_IP_address>\",(0-127)"

static const char *PPP_MOD_ESC = "+++";
static const char *TAG = "pppd";

/* The PPP control block */
static ppp_pcb *ppp;

/* The PPP IP interface */
struct netif ppp_netif;

static bool keep_running = true;

typedef struct at_ppp_config
{
#if PPP_SERVER
    struct ppp_addrs addrs; /* ip4_addr_t our_ipaddr, his_ipaddr, netmask, dns1, dns2; */
#else
#warning PPP_SERVER is disabled!
#endif

    uint8_t options;
} at_ppp_config;

static struct at_ppp_config pppd_config;

bool esp_at_pppd_cmd_regist(void)
{
    return esp_at_custom_cmd_array_regist(at_pppd_cmd, sizeof(at_pppd_cmd) / sizeof(at_pppd_cmd[0]));
}

/**
 * @brief Test Command
 * Queries the Set Commands’ internal parameters and their range of values.
 * Command Format: AT+=?
 **/
uint8_t at_testCmdPpp(uint8_t *cmd_name)
{
    ESP_LOGI(TAG, "at_testCmdPpp: %s", cmd_name);
    char response[((int)strlen((char *)cmd_name) + (int)strlen(PPP_AT_PARAMS)) + 2];

    sprintf(response, "\n%s:%s", cmd_name, PPP_AT_PARAMS);
    uart_write_bytes(CONFIG_AT_UART_PORT, response, strlen(response));

    return ESP_AT_RESULT_CODE_OK;
}

/**
 * @brief Set Command
 * Sets the value of user-defined parameters in commands, and runs these commands.
 * Command Format: AT+=<…>
 **/
uint8_t at_setupCmdPpp(uint8_t para_num)
{
    ESP_LOGE(TAG, "at_setupCmdPpp: %i", para_num);
#if PPP_SERVER
    int32_t cnt = 0;

    char *para_str = NULL;
    int32_t value = 0;

    if (para_num != 2)
        return ESP_AT_CMD_ERROR_PARA_NUM(2, para_num);

    if (esp_at_get_para_as_str(cnt++, (uint8_t **)&para_str) == ESP_AT_PARA_PARSE_RESULT_FAIL)
    {
        ESP_LOGE(TAG, "Parse error on parameter: %d.", cnt - 1);
        return ESP_AT_CMD_ERROR_PARA_TYPE(cnt - 1);
    }
    else
    {
        char *ip_str = NULL;

        ESP_LOGD(TAG, "Raw IPs: %s", para_str);

        if ((ip_str = index(para_str, PPP_IP_DELIM)))
        {
            // Make 2 null terminated strings, one for each IP.
            *ip_str = '\0';
            ip_str++;
            // Try to convert each string to an lwip IP address structure.
            if (!ip4addr_aton(para_str, &pppd_config.addrs.our_ipaddr))
                pppd_config.addrs.our_ipaddr.addr = IPADDR_ANY;

            if (!ip4addr_aton(ip_str, &pppd_config.addrs.his_ipaddr))
                pppd_config.addrs.his_ipaddr.addr = IPADDR_ANY;

            pppd_config.addrs.netmask.addr = IPADDR_BROADCAST;
        }
        else
        {
            ESP_LOGE(TAG, "Parameter %d requires \":\" delimeter.", cnt - 1);
            return ESP_AT_CMD_ERROR_PARA_INVALID(cnt - 1);
        }
    }

    if (esp_at_get_para_as_digit(cnt++, &value) == ESP_AT_PARA_PARSE_RESULT_FAIL)
    {
        ESP_LOGE(TAG, "Parse error on parameter: %d.", cnt - 1);
        return ESP_AT_CMD_ERROR_PARA_TYPE(cnt - 1);
    }
    pppd_config.options = (int8_t)value;

    ESP_LOGI(TAG, "New settings: ");
    ESP_LOGI(TAG, "Local  PPP IP address: %s", ip4addr_ntoa(&pppd_config.addrs.our_ipaddr));
    ESP_LOGI(TAG, "Remote PPP IP address: %s", ip4addr_ntoa(&pppd_config.addrs.his_ipaddr));
    ESP_LOGI(TAG, "Other  PPP options: opt reg: %d", pppd_config.options);
#endif /* PPP_SERVER */

    return ESP_AT_RESULT_CODE_OK;
}

/**
 * @brief Query Command
 * Returns the current value of parameters.
 * Command Format: AT+?
 **/
uint8_t at_queryCmdPpp(uint8_t *cmd_name)
{
    ESP_LOGI(TAG, "at_queryCmdPpp: %s", cmd_name);
    char response[128];

#if PPP_SERVER
    sprintf(response, "\n%s:\"%s:%s\",%d", cmd_name, ip4addr_ntoa(&pppd_config.addrs.his_ipaddr),
            ip4addr_ntoa(&pppd_config.addrs.his_ipaddr), pppd_config.options);
    uart_write_bytes(CONFIG_AT_UART_PORT, response, strlen(response));

#endif /* PPP_SERVER */
    return ESP_AT_RESULT_CODE_OK;
}

/* PPP status callback example */
static void ppp_status_cb(ppp_pcb *pcb, int err_code, void *ctx)
{
    ESP_LOGI(TAG, "ppp_status_cb");
    struct netif *pppif = ppp_netif(pcb);
    strcpy(pppif->name, "p0");
    LWIP_UNUSED_ARG(ctx);

    switch (err_code)
    {
    case PPPERR_NONE:
    {
        ESP_LOGI(TAG, "status_cb: Connected\n");
#if PPP_IPV4_SUPPORT
        ESP_LOGI(TAG, "   our_ipaddr  = %s\n", ipaddr_ntoa(&pppif->ip_addr));
        ESP_LOGI(TAG, "   his_ipaddr  = %s\n", ipaddr_ntoa(&pppif->gw));
        ESP_LOGI(TAG, "   netmask     = %s\n", ipaddr_ntoa(&pppif->netmask));

#endif /* PPP_IPV4_SUPPORT */
#if PPP_IPV6_SUPPORT
        ESP_LOGI(TAG, "   our6_ipaddr = %s\n", ip6addr_ntoa(netif_ip6_addr(pppif, 0)));
#endif /* PPP_IPV6_SUPPORT */

        //pppif->input = *bridge_input_cb;
        break;
    }
    case PPPERR_PARAM:
    {
        ESP_LOGE(TAG, "status_cb: Invalid parameter\n");
        break;
    }
    case PPPERR_OPEN:
    {
        ESP_LOGE(TAG, "status_cb: Unable to open PPP session\n");
        break;
    }
    case PPPERR_DEVICE:
    {
        ESP_LOGE(TAG, "status_cb: Invalid I/O device for PPP\n");
        break;
    }
    case PPPERR_ALLOC:
    {
        ESP_LOGE(TAG, "status_cb: Unable to allocate resources\n");
        break;
    }
    case PPPERR_USER:
    {
        ESP_LOGE(TAG, "status_cb: User interrupt\n");
        break;
    }
    case PPPERR_CONNECT:
    {
        ESP_LOGE(TAG, "status_cb: Connection lost\n");
        break;
    }
    case PPPERR_AUTHFAIL:
    {
        ESP_LOGE(TAG, "status_cb: Failed authentication challenge\n");
        break;
    }
    case PPPERR_PROTOCOL:
    {
        ESP_LOGE(TAG, "status_cb: Failed to meet protocol\n");
        break;
    }
    case PPPERR_PEERDEAD:
    {
        ESP_LOGE(TAG, "status_cb: Connection timeout\n");
        break;
    }
    case PPPERR_IDLETIMEOUT:
    {
        ESP_LOGE(TAG, "status_cb: Idle Timeout\n");
        break;
    }
    case PPPERR_CONNECTTIME:
    {
        ESP_LOGE(TAG, "status_cb: Max connect time reached\n");
        break;
    }
    case PPPERR_LOOPBACK:
    {
        ESP_LOGE(TAG, "status_cb: Loopback detected\n");
        break;
    }
    default:
    {
        ESP_LOGE(TAG, "status_cb: Unknown error code %d\n", err_code);
        break;
    }
    }

    /*
     * This should be in the switch case, this is put outside of the switch
     * case for example readability.
     */

    if (err_code == PPPERR_NONE)
    {
        return;
    }
    else
    {
        keep_running = false;
    }

    /* ppp_close() was previously called, don't reconnect */
    if (err_code == PPPERR_USER)
    {
        /* ppp_free(); -- can be called here */
        return;
    }

    /*
     * Try to reconnect in 30 seconds, if you need a modem chatscript you have
     * to do a much better signaling here ;-)
     */
    //ppp_connect(pcb, 30);
    /* OR ppp_listen(pcb); */
}

static u32_t ppp_output_callback(ppp_pcb *pcb, u8_t *data, u32_t len, void *ctx)
{
    ESP_LOGD(TAG, "PPP tx len %d", len);
    return uart_write_bytes(CONFIG_AT_UART_PORT, (const char *)data, len);
}

/**
 * @brief Execute Command
 * Runs commands with no user-defined parameters.
 * Command Format: AT+
 **/
uint8_t at_exeCmdPpp(uint8_t *cmd_name)
{
    ESP_LOGI(TAG, "at_exeCmdPpp");
    char *data = (char *)malloc(BUF_SIZE);

    keep_running = true;
    // Use the thread-safe pppapi to create a ppp over serial network interface.
    if (ppp == NULL)
    {
        ppp = pppapi_pppos_create(&ppp_netif,
                                  ppp_output_callback, ppp_status_cb, NULL);
        ESP_LOGD(TAG, "After pppapi_pppos_create");
    }

    if (ppp == NULL)
    {
        ESP_LOGD(TAG, "Error init pppos");
        return ESP_AT_RESULT_CODE_ERROR;
    }

#ifndef CONFIG_PPP_SUPPORT
#ifndef PPP_SERVER
    //Set the default interface as ppp - we don't want this as a server.
    pppapi_set_default(ppp);
    //ESP_LOGI(TAG, "After pppapi_set_default");
    //sifaddr(ppp, our, his, nm );
#endif
#endif

    //pppapi_set_auth(ppp, PPPAUTHTYPE_PAP, PPP_User, PPP_Pass);
    //ESP_LOGI(TAG, "After pppapi_set_auth");

#if PPP_SERVER
#if PPP_IPV4_SUPPORT
    pppd_config.addrs.netmask.addr = IPADDR_BROADCAST;
#if LWIP_DNS
    pppd_config.addrs.dns1 = dns_getserver(1)->u_addr.ip4;
#endif /* LWIP_DNS */

#if LWIP_AUTOIP
    if (pppd_config.addrs.our_ipaddr.addr == IPADDR_ANY)
    {
        netifapi_autoip_start(ppp);
    }
#endif

#endif /* PPP_IPV4_SUPPORT */

    //pppapi_listen(ppp, &pppd_config.addrs);
    ppp_set_ipcp_ouraddr(ppp, &pppd_config.addrs.our_ipaddr);
    ppp_set_ipcp_hisaddr(ppp, &pppd_config.addrs.his_ipaddr);
    ppp_set_ipcp_dnsaddr(ppp, 1, &pppd_config.addrs.dns1);
    ppp_set_ipcp_dnsaddr(ppp, 2, &pppd_config.addrs.dns2);

    pppapi_listen(ppp);
    ESP_LOGD(TAG, "After pppapi_listen");
#else
    pppapi_connect(ppp, 0);
    ESP_LOGD(TAG, "After pppapi_connect");
#endif

    uart_write_bytes(CONFIG_AT_UART_PORT, "CONNECT\r\n", strlen("CONNECT\r\n"));

    //esp_at_response_result(ESP_AT_RESULT_CODE_OK);

    dbg_lwip_tcp_pcb_show();

    /**
 * Callback fn
 **/
    //ppp->netif->input = *bridge_input_cb;
    //ppp_netif.input = *bridge_input_cb;

    while (keep_running)
    {
        memset(data, 0, BUF_SIZE);
        int len = uart_read_bytes(CONFIG_AT_UART_PORT, (uint8_t *)data, BUF_SIZE, (portTICK_RATE_MS * 5));

        if ((len == 3) && (memcmp(data, PPP_MOD_ESC, 3) == 0))
        {
            sprintf("%s", data);
            // "+++" input escapes and returns to AT terminal control.
            // You can't type this fast enough. A terminal macro works well though.
            ESP_LOGI(TAG, "Received \"%s\", closing PPP and escaping.", PPP_MOD_ESC);
            return uart_write_bytes(CONFIG_AT_UART_PORT, PPP_MOD_ESC, strlen(PPP_MOD_ESC));

            keep_running = false;
        }
        else if (len > 0)
        {
            ESP_LOGI(TAG, "PPP rx len %d", len);
            pppos_input_tcpip(ppp, (u8_t *)data, len);
        }
    }
#if LWIP_AUTOIP
    netifapi_autoip_stop(ppp);
#endif

    pppapi_close(ppp, 1);

    /* Don't call
    ppp_free(ppp);
    because we can re-use the netif later.*/

    //return ESP_AT_RESULT_CODE_PROCESS_DONE;
    return ESP_AT_RESULT_CODE_OK;
}

#else
#warning CONFIG_PPP_SUPPORT is disabled!
#endif
