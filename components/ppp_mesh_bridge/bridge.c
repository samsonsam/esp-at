#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "lwip/netif.h"
#include "lwip/ip4.h"
#include "lwip/err.h"

static const char *TAG = "bridge";

#define IP_PROTO_ICMP 1
#define IP_PROTO_IGMP 2
#define IP_PROTO_UDP 17
#define IP_PROTO_UDPLITE 136
#define IP_PROTO_TCP 6

err_t input_cb(struct pbuf *p, struct netif *inp)
{
    if (!strcmp(inp->name, "p0") == 0)
    {
        return ERR_IF;
    }
    const struct ip_hdr *iphdr;

    iphdr = (struct ip_hdr *)p->payload;
    switch (iphdr->_proto)
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
        ESP_LOGE(TAG, "Received Packet with unknown type");
        break;
    }

    printf("tot_len: %i\n", p->tot_len);
    printf("payload: 0x%X\n", *(unsigned int *)p->payload);
    pbuf_free(p);
    return ERR_OK;
}

void test()
{
    ESP_LOGI(TAG, "test");
}