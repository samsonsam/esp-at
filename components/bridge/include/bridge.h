#ifndef __BRIDGE_H__
#define __BRIDGE_H__

#include <stdbool.h>
#include "lwip/netif.h"
#include "lwip/ip4.h"
#include "lwip/ip4_frag.h"
#include "lwip/ip4_addr.h"
#include "lwip/ip_addr.h"
#include "lwip/ip.h"
#include "lwip/err.h"
#include "lwip/inet_chksum.h"
#include "lwip/err.h"
#include "esp_mesh.h"


/*******************************************************
 *                Function Declarations
 *******************************************************/
// function for setting ppp netif
void set_ppp_netif(struct netif *ppp);

// function for setting wifi sta netif
void set_wifi_netif(struct netif *wifi);

// function for unsetting ppp netif
void delete_ppp_netif();

// Prints string containing the Protocol name of the given IP header
void print_ip_proto_type(struct ip_hdr *hdr);

// prints out the src and dst address given by IP header
void print_ip_hdr_info(struct ip_hdr *hdr);

/**
 * Sending the provided IP-Frame inside the pbuf's payload over the Wifi STA netif, 
 * but only if this node is the ROOT node of the ESP-MESH.
 * 
 * This function will set the source address of the frame to the Wifi STA's address,
 * so the Distribution System thinks frames are comming directly from the ROOT node.
 * The frames checksum is recalculated afterwards and written into the IP header.
 * 
 * If this node is not the ROOT node, the pbuf is forwarded over the ESP-MESH to the root node via the function
 * esp_mesh_tx_to_root() included in mesh.h.
 * 
 * This function will not free up memory allocated by the pbuf!
 * 
 * @param p pbuf containing an IP-Frame
 * @return ERR_OK = 0 if transmission over either wifi or mesh was successful, can also return ERR_*
 */
err_t ip4_output_over_wifi(struct pbuf *p);

/**
 * Sending the provided IP-Frame inside the pbuf's payload over the PPPoS netif, 
 * but only if the ppp interface is set.
 * 
 * When transmitting over the ppp interface this function will set the destination address of the frame to the ppp client's address,
 * so the client is not dropping the frame.
 * The frames checksum is recalculated afterwards and written into the IP header.
 * 
 * If the ppp interface is not set, the function will try forwarding this packet to a child node via the function
 * esp_mesh_tx_to_child_ppp() included in mesh.h
 * 
 * This function will not free up memory allocated by the pbuf!
 * 
 * @param p pbuf containing an IP-Frame
 * @return ERR_OK = 0 if transmission over either wifi or mesh was successful, can also return ERR_*
 */
err_t ip4_output_over_ppp(struct pbuf *p);

/**
 * This function should only be called inside ip4.c.
 * It will handle the forwarding over the ESP-MESH or over the 
 * bridge netifs (WIFI or PPP)
 * @param rx the received pbuf
 * @param inp the netif the pbuf was received on
 * 
 * If the pbuf was consumed it will free the memory which was allocated by the pbuf.
 * 
 * @return ERR_OK = 0 if the pbuf was forwarded successful
 */
err_t bridge_ip4_output(struct pbuf *rx, struct netif *inp);

#endif //__BRIDGE_H__