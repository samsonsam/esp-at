

#ifndef __MESH_H__
#define __MESH_H__

#include "esp_err.h"
#include "esp_event_base.h"
#include "lwip/pbuf.h"

/*******************************************************
 *                Function Declarations
 *******************************************************/

void print_esp_err_t(char *fncn, esp_err_t err);

/**
 * Sending pbuf over the ESP_MESH to a child node.
 * This functions packs the pbuf's payload into a mesh_data_t struct and forwards
 * to a node where the ppp netif is set.
 * 
 * The function will only send the mesh_data_t if it has received mesh_data_t in advance trough
 * esp_mesh_p2p_rx_main() where "node_with_ppp_interface" is set.
 * 
 * @param p pbuf including the ip header
 * @return ESP_OK = 0 if the packet was transmitted over the ESP_MESH.
 */
esp_err_t esp_mesh_tx_to_child_ppp(struct pbuf *p);

/**
 * Sending pbuf over the esp_mesh to the ROOT node.
 * This functions packs the pbuf's payload into a mesh_data_t struct and forwards
 * to the ROOT node.
 * @param p pbuf including the ip header
 * @return ESP_OK = 0 if the packet was transmitted over th ESP_MESH to ROOT
 */
esp_err_t esp_mesh_tx_to_root(struct pbuf *p);

/**
 * Receiving MESH packets, converting mesh_data_t into pbuf and forwarding to either wifi or ppp netif.
 * If this node is ROOT pbuf will be forwarded over WIFI STA netif to the Distribution System -> ip4_output_over_wifi() (bridge.h)
 * If this node is not ROOT the pbuf will be forwarded over the ppp netif -> ip4_output_over_ppp() (bridge.h)
 * 
 * @param arg not used
 * @return void
 */
void esp_mesh_p2p_rx_main(void *arg);

/**
 * Starting FreeRTOS Task for receiving MESH packets -> esp_mesh_comm_p2p_start()
 * 
 * @param void
 * @return ESP_OK on success
 */
esp_err_t esp_mesh_comm_p2p_start(void);

/**
 * 
 * @arg
 * @param event_base
 * @param event_id
 * @param event_data
 */
void mesh_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

/**
 * 
 * @arg
 * @param event_base
 * @param event_id
 * @param event_data
 */
void ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

/**
 * Initialize ESP_MESH
 */
void mesh_init(void);

#endif /* __MESH_H__ */
