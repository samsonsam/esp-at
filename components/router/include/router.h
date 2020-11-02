

#ifndef __ROUTER_H__
#define __ROUTER_H__

#include "esp_err.h"
#include "esp_event_base.h"

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

void mesh_init(void);

#endif /* __ROUTER_H__ */
