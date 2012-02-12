#include "include/vde3/packet.h"
#ifndef __VDEMEMPOOL
#define __VDEMEMPOOL

vde_pkt *vdepool_pkt_new(int data_size);
void vdepool_pkt_discard(vde_pkt *p);
vde_pkt *vdepool_pkt_new(int data_size);
void vdepool_pkt_discard(vde_pkt *p);

vde_pkt *vdepool_pkt_compact_cpy(vde_pkt *orig);
vde_pkt *vdepool_pkt_cpy(vde_pkt *orig);
vde_pkt *vdepool_pkt_deepcpy(vde_pkt *orig);
vde_pkt *vdepool_pkt_compact_deepcpy(vde_pkt *orig);

void vde_pkt_setprops(vde_pkt *pkt, unsigned int head, unsigned int tail);
vde_pkt *vde_pkt_new(unsigned int payload_sz, unsigned int head, unsigned int tail);

#endif
