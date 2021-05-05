#include <stdio.h>
#include <string.h>

#include "bridge.h"
#include "esp_log.h"
#include "mesh.h"

/*******************************************************
 *                Variable Definitions
 *******************************************************/
// Tag for Logging
static const char *TAG = "bridge";
// the ppp netif
static struct netif *bridge_ppp_netif;

// the wifi sta netif
static struct netif *bridge_wifi_netif;

// boolean indicating if the ppp netif is set
static bool ppp_netif_set = false;

// boolean indicating if the wifi sta netif is set
static bool wifi_netif_set = false;

/*******************************************************
 *                Macros
 *******************************************************/

#define LOG(...) ESP_LOGI(TAG, __VA_ARGS__)

/*******************************************************
 *                Function Definitions
 *******************************************************/

void print_mesh_status()
{
    LOG("esp_mesh_is_root: %s", esp_mesh_is_root() ? "true" : "false");
}

void set_ppp_netif(struct netif *ppp)
{
    LOG("Setting ppp netif: %s", ppp->name);
    LOG("netif address: %s", ip4addr_ntoa((ip4_addr_t *)&ppp->ip_addr));
    bridge_ppp_netif = ppp;
    ppp_netif_set = true;
}

void set_wifi_netif(struct netif *wifi)
{
    LOG("Setting wifi netif: %s", wifi->name);
    LOG("netif address: %s", ip4addr_ntoa((ip4_addr_t *)&wifi->ip_addr));
    bridge_wifi_netif = wifi;
    wifi_netif_set = true;
}

void delete_ppp_netif()
{
    ppp_netif_set = false;
}

void print_ip_proto_type(struct ip_hdr *hdr)
{
    switch (IPH_PROTO(hdr))
    {
    case IP_PROTO_ICMP:
        LOG("Received ICMP Packet");
        break;
    case IP_PROTO_IGMP:
        LOG("Received IGMP Packet");
        break;
    case IP_PROTO_UDP:
        LOG("Received UDP Packet");
        break;
    case IP_PROTO_UDPLITE:
        LOG("Received UDPLITE Packet");
        break;
    case IP_PROTO_TCP:
        LOG("Received TCP Packet");
        break;
    default:
        ESP_LOGE(TAG, "Received Packet with unknown type: %i", hdr->_proto);
    }
    return;
}

void print_ip_hdr_info(struct ip_hdr *hdr)
{
    print_ip_proto_type(hdr);
    LOG("Source Address: %s", ipaddr_ntoa((ip_addr_t *)&hdr->src));
    LOG("Destination Address: %s", ipaddr_ntoa((ip_addr_t *)&hdr->dest));
}

void print_err_t(char *str, err_t err)
{
    if (err != ERR_OK)
    {
        ESP_LOGE(TAG, "%s err: %s\n", str, lwip_strerr(err));
    }
    else
    {
        LOG("%s err: %s\n", str, lwip_strerr(err));
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
    LOG("ip4_output_over_wifi");
    struct ip_hdr *iphdr_rx = (struct ip_hdr *)p->payload;
    print_ip_hdr_info(iphdr_rx);
    err_t ret;
    //print_mesh_status();
    // set the src addr of the tx packet to the wifi interfaces address
    iphdr_rx->src.addr = bridge_wifi_netif->ip_addr.u_addr.ip4.addr;
    // calculate the new checksum
    IPH_CHKSUM_SET(iphdr_rx, inet_chksum(iphdr_rx, p->len));
    //iphdr_rx->_chksum = inet_chksum(iphdr_rx, p->len);
    ret = ip4_output_if(p, (ip4_addr_t *)&iphdr_rx->src, LWIP_IP_HDRINCL, IPH_TTL(iphdr_rx), IPH_TOS(iphdr_rx), IPH_PROTO(iphdr_rx), bridge_wifi_netif);
    print_err_t("ip4_output_over_wifi", ret);
    return ret;
}

err_t ip4_output_over_ppp(struct pbuf *p)
{
    LOG("ip4_output_over_ppp");
    err_t ret;
    struct ip_hdr *iphdr_rx = (struct ip_hdr *)p->payload;
    print_ip_hdr_info(iphdr_rx);
    //print_mesh_status();
    iphdr_rx->dest.addr = bridge_ppp_netif->gw.u_addr.ip4.addr;
    // calculate the new checksum
    IPH_CHKSUM_SET(iphdr_rx, inet_chksum(iphdr_rx, p->len));
    //iphdr_rx->_chksum = inet_chksum(iphdr_rx, p->len);
    ret = ip4_output_if(p, (ip4_addr_t *)&iphdr_rx->src, LWIP_IP_HDRINCL, IPH_TTL(iphdr_rx), IPH_TOS(iphdr_rx), IPH_PROTO(iphdr_rx), bridge_ppp_netif);
    print_err_t("ip4_output_over_ppp", ret);
    return ret;
}

err_t bridge_ip4_output(struct pbuf *rx, struct netif *inp)
{
    err_t ret;
    LOG("netif: %s", inp->name);
    LOG("netif address: %s", ip4addr_ntoa((ip4_addr_t *)&inp->ip_addr));
    print_ip_hdr_info((struct ip_hdr *)rx->payload);

    // Packet received over ppp
    if (is_ppp_netif(inp) == 0)
    {
        if (!esp_mesh_is_root())
            ret = esp_mesh_tx_to_root(rx);
        else
            ret = ip4_output_over_wifi(rx);
    }
    // packet received over wifi
    else if (is_wifi_netif(inp) == 0)
    {
        if (ppp_netif_set == false)
            ret = esp_mesh_tx_to_child_ppp(rx);
        else
            ret = ip4_output_over_ppp(rx);
    }
    else
    {
        ESP_LOGE(TAG, "No matching netif registered for received pbuf!");
        ret = ERR_IF;
    }

    if (ret == ERR_OK)
    {
        pbuf_free(rx);
    }

    printf("\n");
    return ret;
}