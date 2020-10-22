#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "lwip/netif.h"
#include "lwip/ip4.h"
#include "lwip/err.h"
#include "esp_mesh.h"
#include "lwip/ip4_frag.h"
#include "lwip/ip4_addr.h"
#include "lwip/ip4_addr.h"
#include "lwip/inet_chksum.h"

static const char *TAG = "bridge";

static struct netif *ppp;

static ip4_addr_p_t ppp_client;

static int8_t ppp_client_set = 0;

#define IP_PROTO_ICMP 1
#define IP_PROTO_IGMP 2
#define IP_PROTO_UDP 17
#define IP_PROTO_UDPLITE 136
#define IP_PROTO_TCP 6

void print_ip_proto_type(struct ip_hdr *hdr)
{
    switch (hdr->_proto)
    {
    case IP_PROTO_ICMP:
        ESP_LOGD(TAG, "Received ICMP Packet");
        break;
    case IP_PROTO_IGMP:
        ESP_LOGD(TAG, "Received IGMP Packet");
        break;
    case IP_PROTO_UDP:
        ESP_LOGD(TAG, "Received UDP Packet");
        break;
    case IP_PROTO_UDPLITE:
        ESP_LOGD(TAG, "Received UDPLITE Packet");
        break;
    case IP_PROTO_TCP:
        ESP_LOGD(TAG, "Received TCP Packet");
        break;
    default:
        ESP_LOGE(TAG, "Received Packet with unknown type: %i", hdr->_proto);
    }
    return;
}

void print_ip_hdr_info(struct ip_hdr *hdr)
{
    ESP_LOGI(TAG, "Source Address: %s", ipaddr_ntoa((ip_addr_t *)&hdr->src));
    ESP_LOGI(TAG, "Destination Address: %s", ipaddr_ntoa((ip_addr_t *)&hdr->dest));
    print_ip_proto_type(hdr);
}

err_t bridge_input_cb_ppp(struct pbuf *p, struct netif *inp)
{
    if (strcmp(inp->name, "p0") != 0)
    {
        return ERR_IF;
    }
    ESP_LOGD(TAG, "bridge_input_cb_ppp");
    struct ip_hdr *iphdr;
    err_t ret;

    iphdr = (struct ip_hdr *)p->payload;
    print_ip_proto_type(iphdr);

    // if(esp_mesh_is_root())
    // {
    //     // Send packet over mesh to root
    //     return ERR_OK;
    // }

    ppp = inp;
    ppp_client = iphdr->src;
    ppp_client_set = 1;
    iphdr->src.addr = &ppp->ip_addr.u_addr.ip4.addr;
    iphdr->src = ppp_client;
    iphdr->_chksum = inet_chksum(iphdr, p->len);

    ret = ip4_output(p, &ppp->ip_addr, &ppp_client, iphdr->_ttl, iphdr->_tos, iphdr->_proto);
    ESP_LOGD(TAG, "after ip4_output; err: %i", ret);
    pbuf_free(p);
    return ret;
}

err_t bridge_input_cb_wifi(struct pbuf *p, struct netif *inp)
{
    if (strcmp(inp->name, "p0") == 0)
    {
        return ERR_IF;
    }
    ESP_LOGD(TAG, "bridge_input_cb_wifi");
    struct ip_hdr *iphdr;
    err_t ret;

    iphdr = (struct ip_hdr *)p->payload;
    print_ip_proto_type(iphdr);

    if (ppp_client_set == 0)
    {
        return ERR_RTE;
    }
    


    iphdr->src.addr = &ppp->ip_addr.u_addr.ip4.addr;
    iphdr->src = ppp_client;
    iphdr->_chksum = inet_chksum(iphdr, p->len);

    ret = ip4_output(p, &ppp->ip_addr, &ppp_client, iphdr->_ttl, iphdr->_tos, iphdr->_proto);
    ESP_LOGD(TAG, "after ip4_output; err: %i", ret);

    if (ret == ERR_RTE)
    {
        /* code */
    }
    


    pbuf_free(p);
    return ret;
}