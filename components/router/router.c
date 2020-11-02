// Copyright 2017 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "mdf_common.h"
#include "mwifi.h"
#include "driver/uart.h"

#include "bridge.h"
#include "esp_mesh.h"
#include "lwip/pbuf.h"
#include "lwip/ip4.h"
#include "mdf_err.h"

// #define MEMORY_DEBUG

#define BUF_SIZE (1024)
#define RX_SIZE MWIFI_PAYLOAD_LEN
#define TX_SIZE (1460)

static const char *TAG = "router_example";
//static esp_netif_t *netif_sta = NULL;

static uint8_t node_with_ppp_interface;
static int8_t node_with_ppp_interface_is_set = 0;



esp_err_t esp_mesh_tx_to_child_ppp(struct pbuf *p)
{
    esp_err_t ret;
    mwifi_data_type_t *data_type;

    data_type->compression = false;
    data_type->upgrade = false;
    data_type->communicate = MWIFI_COMMUNICATE_UNICAST;
    data_type->group = false;

    if (node_with_ppp_interface_is_set == 0) return ESP_ERR_MESH_NO_ROUTE_FOUND;
    ret = mwifi_write(node_with_ppp_interface, data_type, p->payload, sizeof(p->payload), false);
    ESP_LOGI(TAG, "esp_mesh_tx_to_child_ppp -> error: %s", mdf_err_to_name(ret));
    if (ret != 0)
    {
        return ERR_VAL;
    }
    return ret;
}

esp_err_t esp_mesh_tx_to_root(struct pbuf *p)
{
    esp_err_t ret;
    mwifi_data_type_t *data_type;

    data_type->compression = false;
    data_type->upgrade = false;
    data_type->communicate = MWIFI_COMMUNICATE_UNICAST;
    data_type->group = false;

    ESP_LOGI(TAG, "sending packet to root");
    ret = mwifi_write(NULL, data_type, p->payload, sizeof(p->payload), false);
    ESP_LOGI(TAG, "esp_mesh_tx_to_root -> error: %s", mdf_err_to_name(ret));
    if (ret != 0)
    {
        return ERR_VAL;
    }
    return ret;
}


static void node_read_task(void *arg)
{
    mdf_err_t ret                    = MDF_OK;
    char *data                       = MDF_MALLOC(MWIFI_PAYLOAD_LEN);
    size_t size                      = MWIFI_PAYLOAD_LEN;
    mwifi_data_type_t data_type      = {0x0};
    uint8_t src_addr[MWIFI_ADDR_LEN] = {0x0};

    MDF_LOGI("Note read task is running");

    for (;;) {
        if (!mwifi_is_connected()) {
            vTaskDelay(500 / portTICK_RATE_MS);
            continue;
        }

        size = MWIFI_PAYLOAD_LEN;
        memset(data, 0, MWIFI_PAYLOAD_LEN);
        ret = mwifi_read(src_addr, &data_type, data, &size, portMAX_DELAY);
        MDF_ERROR_CONTINUE(ret != MDF_OK, "<%s> mwifi_read", mdf_err_to_name(ret));
        MDF_LOGD("Node receive: " MACSTR ", size: %d", MAC2STR(src_addr), size);

        if (node_with_ppp_interface_is_set == 0)
        {
            //node_with_ppp_interface = &from;
            node_with_ppp_interface = src_addr;
            node_with_ppp_interface_is_set = 1;
        }

        struct pbuf *q;
        esp_err_t ret;
        struct ip_hdr *iphdr = (struct ip_hdr *) data;
        q = pbuf_alloc(PBUF_RAW, iphdr->_len, PBUF_REF);
        q->payload = data;
        //ret = create_pbuf_from_received_mesh_packet(q, &data);
        if (esp_mesh_is_root())
        {
            ret = ip4_output_over_wifi(q);
            ESP_LOGI(TAG, "Received pbuf over esp-mesh: forwarded over wifi -> err: %i", ret);
        }
        else
        {
            ret = ip4_output_over_ppp(q);
            ESP_LOGI(TAG, "Received pbuf over esp-mesh: forwarded over ppp -> err: %i", ret);
        }

    }

    MDF_LOGW("Note read task is exit");

    MDF_FREE(data);
    vTaskDelete(NULL);
}

static void node_write_task(void *arg)
{
    size_t size                     = 0;
    int count                       = 0;
    char *data                      = NULL;
    mdf_err_t ret                   = MDF_OK;
    mwifi_data_type_t data_type     = {0};
    uint8_t sta_mac[MWIFI_ADDR_LEN] = {0};

    MDF_LOGI("NODE task is running");

    esp_wifi_get_mac(ESP_IF_WIFI_STA, sta_mac);

    for (;;) {
        if (!mwifi_is_connected()) {
            vTaskDelay(500 / portTICK_RATE_MS);
            continue;
        }

        size = asprintf(&data, "{\"src_addr\": \"" MACSTR "\",\"data\": \"Hello TCP Server!\",\"count\": %d}",
                        MAC2STR(sta_mac), count++);

        MDF_LOGD("Node send, size: %d, data: %s", size, data);
        ret = mwifi_write(NULL, &data_type, data, size, true);
        MDF_FREE(data);
        MDF_ERROR_CONTINUE(ret != MDF_OK, "<%s> mwifi_write", mdf_err_to_name(ret));

        vTaskDelay(3000 / portTICK_RATE_MS);
    }

    MDF_FREE(data);
    MDF_LOGW("NODE task is exit");

    vTaskDelete(NULL);
}

/**
 * @brief Timed printing system information
 */
static void print_system_info_timercb(void *timer)
{
    uint8_t primary                 = 0;
    wifi_second_chan_t second       = 0;
    mesh_addr_t parent_bssid        = {0};
    uint8_t sta_mac[MWIFI_ADDR_LEN] = {0};
    wifi_sta_list_t wifi_sta_list   = {0x0};

    esp_wifi_get_mac(ESP_IF_WIFI_STA, sta_mac);
    esp_wifi_ap_get_sta_list(&wifi_sta_list);
    esp_wifi_get_channel(&primary, &second);
    esp_mesh_get_parent_bssid(&parent_bssid);

    MDF_LOGI("System information, channel: %d, layer: %d, self mac: " MACSTR ", parent bssid: " MACSTR
             ", parent rssi: %d, node num: %d, free heap: %u", primary,
             esp_mesh_get_layer(), MAC2STR(sta_mac), MAC2STR(parent_bssid.addr),
             mwifi_get_parent_rssi(), esp_mesh_get_total_node_num(), esp_get_free_heap_size());

    for (int i = 0; i < wifi_sta_list.num; i++) {
        MDF_LOGI("Child mac: " MACSTR, MAC2STR(wifi_sta_list.sta[i].mac));
    }

#ifdef MEMORY_DEBUG

    if (!heap_caps_check_integrity_all(true)) {
        MDF_LOGE("At least one heap is corrupt");
    }

    mdf_mem_print_heap();
    mdf_mem_print_record();
    mdf_mem_print_task();
#endif /**< MEMORY_DEBUG */
}

static mdf_err_t wifi_init()
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();


    //MDF_ERROR_ASSERT(esp_netif_init());
    //MDF_ERROR_ASSERT(esp_event_loop_create_default());
    //ESP_ERROR_CHECK(esp_netif_create_default_wifi_mesh_netifs(&netif_sta, NULL));
    MDF_ERROR_ASSERT(esp_event_loop_init(NULL, NULL));
    MDF_ERROR_ASSERT(esp_wifi_init(&cfg));
    MDF_ERROR_ASSERT(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
    MDF_ERROR_ASSERT(esp_wifi_set_mode(WIFI_MODE_STA));
    MDF_ERROR_ASSERT(esp_wifi_set_ps(WIFI_PS_NONE));
    MDF_ERROR_ASSERT(esp_mesh_set_6m_rate(false));
    MDF_ERROR_ASSERT(esp_wifi_start());

    return MDF_OK;
}

/**
 * @brief All module events will be sent to this task in esp-mdf
 *
 * @Note:
 *     1. Do not block or lengthy operations in the callback function.
 *     2. Do not consume a lot of memory in the callback function.
 *        The task memory of the callback function is only 4KB.
 */
static mdf_err_t event_loop_cb(mdf_event_loop_t event, void *ctx)
{
    MDF_LOGI("event_loop_cb, event: %d", event);

    switch (event) {
        case MDF_EVENT_MWIFI_STARTED:
            MDF_LOGI("MESH is started");
            break;

        case MDF_EVENT_MWIFI_PARENT_CONNECTED:
            MDF_LOGI("Parent is connected on station interface");

            if (esp_mesh_is_root()) {
                esp_netif_dhcpc_start(netif_sta);
            }

            break;

        case MDF_EVENT_MWIFI_PARENT_DISCONNECTED:
            MDF_LOGI("Parent is disconnected on station interface");
            break;

        case MDF_EVENT_MWIFI_ROUTING_TABLE_ADD:
        case MDF_EVENT_MWIFI_ROUTING_TABLE_REMOVE:
            MDF_LOGI("total_num: %d", esp_mesh_get_total_node_num());
            break;

        case MDF_EVENT_MWIFI_ROOT_GOT_IP: {
            MDF_LOGI("Root obtains the IP address. It is posted by LwIP stack automatically");
            xTaskCreate(tcp_client_write_task, "tcp_client_write_task", 4 * 1024,
                        NULL, CONFIG_MDF_TASK_DEFAULT_PRIOTY, NULL);
            xTaskCreate(tcp_client_read_task, "tcp_server_read", 4 * 1024,
                        NULL, CONFIG_MDF_TASK_DEFAULT_PRIOTY, NULL);
            break;
        }

        default:
            break;
    }

    return MDF_OK;
}

void mesh_init(void)
{
    mwifi_init_config_t cfg   = MWIFI_INIT_CONFIG_DEFAULT();
    mwifi_config_t config     = {
        .router_ssid     = CONFIG_ROUTER_SSID,
        .router_password = CONFIG_ROUTER_PASSWORD,
        .mesh_id         = CONFIG_MESH_ID,
        .mesh_password   = CONFIG_MESH_PASSWORD,
    };

    /**
     * @brief Set the log level for serial port printing.
     */
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set(TAG, ESP_LOG_DEBUG);

    /**
     * @brief Initialize wifi mesh.
     */
    MDF_ERROR_ASSERT(mdf_event_loop_init(event_loop_cb));
    MDF_ERROR_ASSERT(wifi_init());
    MDF_ERROR_ASSERT(mwifi_init(&cfg));
    MDF_ERROR_ASSERT(mwifi_set_config(&config));
    MDF_ERROR_ASSERT(mwifi_start());

    /**
     * @brief select/extend a group memebership here
     *      group id can be a custom address
     */
    const uint8_t group_id_list[2][6] = {{0x01, 0x00, 0x5e, 0xae, 0xae, 0xae},
                                        {0x01, 0x00, 0x5e, 0xae, 0xae, 0xaf}};

    MDF_ERROR_ASSERT(esp_mesh_set_group_id((mesh_addr_t *)group_id_list,
                                           sizeof(group_id_list) / sizeof(group_id_list[0])));

    /**
     * @breif Create handler
     */
    //xTaskCreate(node_write_task, "node_write_task", 4 * 1024, NULL, CONFIG_MDF_TASK_DEFAULT_PRIOTY, NULL);
    xTaskCreate(node_read_task, "node_read_task", 4 * 1024, NULL, CONFIG_MDF_TASK_DEFAULT_PRIOTY, NULL);

    TimerHandle_t timer = xTimerCreate("print_system_info", 10000 / portTICK_RATE_MS,
                                       true, NULL, print_system_info_timercb);
    xTimerStart(timer, 0);
}
