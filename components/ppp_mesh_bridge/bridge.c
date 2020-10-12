#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"

#include "lwip/raw.h"

static const char *TAG = "bridge";


u8_t raw_recv_fn_cb(void *arg, struct raw_pcb *pcb, struct pbuf *p, const ip_addr_t *addr)
{
    ESP_LOGI(TAG, "bridge_cb");
    sprintf("%s", p->payload);
    return 0;
}