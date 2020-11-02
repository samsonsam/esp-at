#ifndef __BRIDGE_H__
#define __BRIDGE_H__

#include "esp_mesh.h"
#include "lwip/err.h"

void set_ppp_netif(struct netif *ppp);

void set_wifi_netif(struct netif *wifi);

void delete_ppp_netif();

void print_ip_proto_type(struct ip_hdr *hdr);

void print_ip_hdr_info(struct ip_hdr *hdr);

err_t ip4_output_over_wifi(struct pbuf *p);

err_t ip4_output_over_ppp(struct pbuf *p);

err_t bridge_ip4_output(struct pbuf *rx, struct netif *inp);

err_t bridge_input_cb_ppp(struct pbuf *p, struct netif *inp);

err_t bridge_input_cb_wifi(struct pbuf *p, struct netif *inp);

err_t create_pbuf_from_received_mesh_packet(struct pbuf *p, mesh_data_t *data);

#endif //__BRIDGE_H__