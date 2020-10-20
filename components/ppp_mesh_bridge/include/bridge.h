#ifndef __BRIDGE_H__
#define __BRIDGE_H__

#include "lwip/raw.h"




err_t input_cb(struct pbuf *p, struct netif *inp);

void test();


#endif //__BRIDGE_H__