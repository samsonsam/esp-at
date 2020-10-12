#ifndef __BRIDGE_H__
#define __BRIDGE_H__

#include "lwip/raw.h"



u8_t raw_recv_fn_cb(void *arg, struct raw_pcb *pcb, struct pbuf *p, const ip_addr_t *addr);





#endif //__BRIDGE_H__