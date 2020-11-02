

#ifndef __MESH_H__
#define __MESH_H__

#include "esp_err.h"
#include "esp_event_base.h"

void esp_mesh_p2p_tx_main(void *arg);

void esp_mesh_p2p_rx_main(void *arg);

/**
 * Sending pbuf over the esp_mesh to the root node
 * @param p pbuf including the ip header
 */
esp_err_t esp_mesh_tx_to_child_ppp(struct pbuf *p);

/**
 * Sending pbuf over the esp_mesh to the node with a ppp interface
 * @param p pbuf including the ip header
 */
esp_err_t esp_mesh_tx_to_root(struct pbuf *p);

esp_err_t esp_mesh_comm_p2p_start(void);

void mesh_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

void ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

void mesh_init(void);

#endif /* __MESH_LIGHT_H__ */
