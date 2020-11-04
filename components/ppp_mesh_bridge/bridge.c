#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "lwip/netif.h"
#include "lwip/ip.h"
#include "lwip/ip4.h"
#include "esp_mesh.h"
#include "lwip/ip4_frag.h"
#include "lwip/ip4_addr.h"
#include "lwip/ip_addr.h"
#include "lwip/inet_chksum.h"
#include "mesh.h"

static const char *TAG = "bridge";

static struct netif *bridge_ppp_netif;

static struct netif *bridge_wifi_netif;

static int8_t ppp_netif_set = 0;

static int8_t wifi_netif_set = 0;

void print_mesh_status()
{
    if (esp_mesh_is_root())
    {
        ESP_LOGI(TAG, "esp_mesh_is_root: true");
    }
    else
    {
        ESP_LOGI(TAG, "esp_mesh_is_root: false");
    }
}

void set_ppp_netif(struct netif *ppp)
{
    ESP_LOGI(TAG, "Setting ppp netif: %s", ppp->name);
    ESP_LOGI(TAG, "netif address: %s", ip4addr_ntoa(&ppp->ip_addr));
    bridge_ppp_netif = ppp;
    ppp_netif_set = 1;
}

void set_wifi_netif(struct netif *wifi)
{
    ESP_LOGI(TAG, "Setting wifi netif: %s", wifi->name);
    ESP_LOGI(TAG, "netif address: %s", ip4addr_ntoa(&wifi->ip_addr));
    bridge_wifi_netif = wifi;
    wifi_netif_set = 1;
}

void delete_ppp_netif()
{
    ppp_netif_set = 0;
}

void print_ip_proto_type(struct ip_hdr *hdr)
{
    switch (hdr->_proto)
    {
    case IP_PROTO_ICMP:
        ESP_LOGI(TAG, "Received ICMP Packet");
        break;
    case IP_PROTO_IGMP:
        ESP_LOGI(TAG, "Received IGMP Packet");
        break;
    case IP_PROTO_UDP:
        ESP_LOGI(TAG, "Received UDP Packet");
        break;
    case IP_PROTO_UDPLITE:
        ESP_LOGI(TAG, "Received UDPLITE Packet");
        break;
    case IP_PROTO_TCP:
        ESP_LOGI(TAG, "Received TCP Packet");
        break;
    default:
        ESP_LOGE(TAG, "Received Packet with unknown type: %i", hdr->_proto);
    }
    return;
}

void print_ip_hdr_info(struct ip_hdr *hdr)
{
    print_ip_proto_type(hdr);
    ESP_LOGI(TAG, "Source Address: %s", ipaddr_ntoa((ip_addr_t *)&hdr->src));
    ESP_LOGI(TAG, "Destination Address: %s", ipaddr_ntoa((ip_addr_t *)&hdr->dest));
}

void print_err_t(char *str, err_t err)
{
    if (err != ERR_OK)
    {
        ESP_LOGE(TAG, "%s err: %s\n", str, lwip_strerr(err));
    }
    else
    {
        ESP_LOGI(TAG, "%s err: %s\n", str, lwip_strerr(err));
    }
}

int8_t is_wifi_netif(struct netif *inp)
{
    if (wifi_netif_set == 1)
    {
        return strcmp(bridge_wifi_netif->name, inp->name);
    }
    return 1;
}

int8_t is_ppp_netif(struct netif *inp)
{
    if (ppp_netif_set == 1)
    {
        return strcmp(bridge_ppp_netif->name, inp->name);
    }
    return 1;
}

err_t ip4_output_over_wifi(struct pbuf *p)
{
    ESP_LOGI(TAG, "ip4_output_over_wifi");
    struct ip_hdr *iphdr_rx = (struct ip_hdr *)p->payload;
    print_ip_hdr_info(iphdr_rx);
    err_t ret;
    print_mesh_status();

    if (!esp_mesh_is_root())
    {
        ret = esp_mesh_tx_to_root(p);
        if (ret == ERR_OK)
            pbuf_free(p);
        return ret;
    }

    // set the src addr of the tx packet to the wifi interfaces address
    iphdr_rx->src.addr = bridge_wifi_netif->ip_addr.u_addr.ip4.addr;
    // calculate the new checksum
    iphdr_rx->_chksum = inet_chksum(iphdr_rx, p->len);

    //u16_t iphdr_hlen;
    //iphdr_hlen = IPH_HL_BYTES(iphdr_rx);
    //pbuf_remove_header(p, IPH_HL_BYTES(iphdr_rx));
    //ret = ip4_output(p, &iphdr_rx->src, &iphdr_rx->dest, iphdr_rx->_ttl, iphdr_rx->_tos, iphdr_rx->_proto);

    ret = ip4_output_if(p, &iphdr_rx->src, LWIP_IP_HDRINCL, iphdr_rx->_ttl, iphdr_rx->_tos, iphdr_rx->_proto, bridge_wifi_netif);
    if (ret == ERR_OK)
        pbuf_free(p);
    print_err_t("ip4_output_over_wifi", ret);
    return ret;
}

err_t ip4_output_over_ppp(struct pbuf *p)
{
    ESP_LOGI(TAG, "ip4_output_over_ppp");
    err_t ret;
    struct ip_hdr *iphdr_rx = (struct ip_hdr *)p->payload;
    print_ip_hdr_info(iphdr_rx);
    print_mesh_status();

    if (ppp_netif_set == 0)
    {
        ret = esp_mesh_tx_to_child_ppp(p);
        if (ret == ERR_OK)
            pbuf_free(p);
        return ret;
    }

    iphdr_rx->dest.addr = bridge_ppp_netif->gw.u_addr.ip4.addr;
    // calculate the new checksum
    iphdr_rx->_chksum = inet_chksum(iphdr_rx, p->len);
    ret = ip4_output_if(p, &iphdr_rx->src, LWIP_IP_HDRINCL, iphdr_rx->_ttl, iphdr_rx->_tos, iphdr_rx->_proto, bridge_ppp_netif);
    if (ret == ERR_OK)
        pbuf_free(p);
    print_err_t("ip4_output_over_ppp", ret);
    return ret;
}

err_t bridge_ip4_output(struct pbuf *rx, struct netif *inp)
{
    ESP_LOGI(TAG, "netif: %s", inp->name);
    ESP_LOGI(TAG, "netif address: %s", ip4addr_ntoa(&inp->ip_addr));
    if (is_ppp_netif(inp) == 0)
    {
        ip4_output_over_wifi(rx);
    }
    else if (is_wifi_netif(inp) == 0)
    {
        return ip4_output_over_ppp(rx);
    }
    // else
    // {
    //     ESP_LOGE(TAG, "No matching netif registered for received pbuf!");
    //     return ERR_IF;
    // }

    printf("\n");
    return ERR_OK;
}

/**
 *
 * @param rx pbuf of incomming packet
 * @param inp netif on which the pbuf was received
 * 
 * @return ERR_OK if transmission to ppp was successfull. in this case the pbuf was freed
 */
err_t bridge_input_cb_wifi(struct pbuf *rx, struct netif *inp)
{
    // check if the interface name equals p0
    if (strcmp(inp->name, "p0") == 0)
    {
        return ERR_IF;
    }
    printf("\n\n");
    ESP_LOGD(TAG, "bridge_input_cb_wifi");

    // if bridge_ppp_netif connection is not established return ERR_IF
    if (ppp_netif_set == 0)
    {
        ESP_LOGD(TAG, "ppp interface not present: returning");
        return ERR_IF;
    }

    struct ip_hdr *iphdr_rx = (struct ip_hdr *)rx->payload;
    struct ip_hdr *iphdr_tx;
    // outgoing pbuf
    struct pbuf *tx;
    err_t ret;

    tx = pbuf_alloc(PBUF_RAW_TX, sizeof(rx->payload), PBUF_RAM);
    tx->payload = rx->payload;
    //tx->l2_owner = inp;
    tx->len = rx->len;
    tx->tot_len = rx->tot_len;
    tx->flags = rx->flags;
    tx->type_internal = rx->type_internal;
    tx->next = rx->next;

    iphdr_tx = (struct ip_hdr *)rx->payload;

    if (iphdr_tx->dest.addr == inp->ip_addr.u_addr.ip4.addr)
    {
        /* This packet is addressed to wifi netif -> exiting */
        return ERR_IF;
    }

    print_ip_hdr_info(iphdr_tx);
    printf("--------------------\n");

    // Setting the source address to the address of the ppp interface
    //iphdr->src.addr = bridge_ppp_netif->ip_addr.u_addr.ip4.addr;
    // Setting the destination address to the address of the ppp client
    iphdr_tx->dest.addr = bridge_ppp_netif->gw.u_addr.ip4.addr;
    // calculate crc checksum
    iphdr_tx->_chksum = inet_chksum(iphdr_tx, tx->len);

    print_ip_hdr_info(iphdr_tx);
    //ip4_debug_print(p);

    ret = ip4_output(tx, &iphdr_tx->src, &iphdr_tx->dest, iphdr_tx->_ttl, iphdr_tx->_tos, iphdr_tx->_proto);
    if (ret != ERR_OK)
    {
        ESP_LOGE(TAG, "bridge_input_cb_wifi -> err: %s\n", lwip_strerr(ret));
    }
    else
    {
        ESP_LOGI(TAG, "bridge_input_cb_wifi -> err: %s\n", lwip_strerr(ret));
    }

    if (ret == ERR_OK)
    {
        pbuf_free(rx);
    }
    pbuf_free(tx);

    return ret;
}

err_t bridge_input_cb_ppp(struct pbuf *rx, struct netif *inp)
{
    if (strcmp(inp->name, "p0") != 0)
    {
        return bridge_input_cb_wifi(rx, inp);
    }

    printf("\n\n");
    ESP_LOGD(TAG, "bridge_input_cb_ppp");
    struct ip_hdr *iphdr_rx = (struct ip_hdr *)rx->payload;
    struct ip_hdr *iphdr_tx;
    struct pbuf *tx;
    err_t ret;

    if (!esp_mesh_is_root())
    {
        // Send packet over mesh to root
        ESP_LOGD(TAG, "Send packet over mesh to root");
        // TODO
        return ERR_OK;
    }

    // allocation memory for the tx pbuf
    tx = pbuf_alloc(PBUF_RAW_TX, sizeof(rx->payload), PBUF_RAM);
    // copying the payload: rx -> tx
    tx->payload = rx->payload;
    iphdr_tx = (struct ip_hdr *)tx->payload;
    print_ip_hdr_info(iphdr_tx);
    printf("--------------------\n");

    //tx->l2_owner = inp;
    tx->len = rx->len;
    tx->tot_len = rx->tot_len;
    //tx->flags = rx->flags;
    //tx->type_internal = rx->type_internal;
    tx->next = rx->next;

    // if the packet is addressed to the ppp interface do nothing here
    if (iphdr_tx->dest.addr == inp->ip_addr.u_addr.ip4.addr)
    {
        /* This packet is addressed to ppp netif -> exiting */
        return ERR_IF;
    }

    // set the src addr of the tx packet to the wifi interfaces address
    iphdr_tx->src.addr = bridge_wifi_netif->ip_addr.u_addr.ip4.addr;
    // calculate the new checksum
    iphdr_tx->_chksum = inet_chksum(iphdr_tx, tx->len);

    print_ip_hdr_info(iphdr_tx);
    ret = ip4_output(tx, &iphdr_tx->src, &iphdr_tx->dest, iphdr_tx->_ttl, iphdr_tx->_tos, iphdr_tx->_proto);

    if (ret != ERR_OK)
    {
        ESP_LOGE(TAG, "bridge_input_cb_wifi -> err: %s\n", lwip_strerr(ret));
    }
    else
    {
        ESP_LOGI(TAG, "bridge_input_cb_wifi -> err: %s\n", lwip_strerr(ret));
    }

    if (ret == ERR_OK)
    {
        pbuf_free(rx);
    }
    pbuf_free(tx);
    return ret;
}

void create_pbuf_from_received_mesh_packet(struct pbuf *p, mesh_data_t *data)
{
    if (data->proto != MESH_PROTO_BIN)
        return ESP_ERR_MESH_NOT_SUPPORT;
    p->payload = data->data;
    p->l2_owner = NULL;
    return ESP_OK;
}