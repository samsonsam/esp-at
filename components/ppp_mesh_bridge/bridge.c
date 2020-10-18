#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "lwip/netif.h"

static const char *TAG = "bridge";


signed char input_cb(struct pbuf *p, struct netif *inp)
{
    ESP_LOGI(TAG, "bridge_cb");
    sprintf("%s", p->payload);
    pbuf_free(p);
    return 1;
}

void test()
{
    ESP_LOGI(TAG, "test");
}