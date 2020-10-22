#ifndef __BRIDGE_H__
#define __BRIDGE_H__

void print_ip_proto_type(struct ip_hdr *hdr);

void print_ip_hdr_info(struct ip_hdr *hdr);

err_t bridge_input_cb_ppp(struct pbuf *p, struct netif *inp);

err_t bridge_input_cb_wifi(struct pbuf *p, struct netif *inp);

#endif //__BRIDGE_H__