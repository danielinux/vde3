#include "include/vde3/packet.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <pthread.h>
#define POOL_GROWTH 128 /* In packets, must be divisible by 8 */

struct vdepool
{
	uint32_t size; /*< In number of packets */
	uint32_t bmp_size;
	uint8_t  *bmp;
	struct vdepool *next;
	uint32_t free_count;
	uint8_t  content[0];
};

typedef struct vdepool vdepool;

static vdepool *pool = NULL;
static vdepool *packets = NULL;

static pthread_mutex_t mempool_lock, references_lock;

/* Allocate a new slice for the pool.
 * Must be already locked when entering this.
 */
static int vdepool_alloc(struct vdepool **list, int elem_size, uint32_t size)
{
	uint32_t total_size = size * elem_size + sizeof(struct vdepool);
	uint32_t bmp_size;
	struct vdepool *slice;

	while(size % 8)
		size++;

	total_size = size * elem_size + sizeof(struct vdepool);
	bmp_size = (size >> 3);

	slice = malloc(total_size);
	if (!slice)
		return -ENOMEM;

	slice->bmp = malloc(bmp_size);
	if (!slice->bmp) {
		free(slice);
		return -ENOMEM;
	}
	memset(slice->bmp, 0, bmp_size);
	slice->size = size;
	slice->bmp_size = bmp_size;
	slice->free_count = size;
	slice->next = *list;
	*list = slice;
	return 0;
}

static inline void set_bit(struct vdepool *p, uint32_t pos)
{
	int i = pos / 8,
		j = pos % 8;
	p->bmp[i] |= (1 << j);
	p->free_count--;
}

static inline void unset_bit(struct vdepool *p, uint32_t pos)
{
	int i = pos / 8,
		j = pos % 8;
	p->bmp[i] &= ~(1 << j);
	p->free_count++;
}

static vde_pkt *reference_alloc(void)
{
	int i, j;
	vde_pkt *p = NULL;
	int position = -1;
	struct vdepool *cur;
	int counter=0;

	pthread_mutex_lock(&references_lock);
	if (!packets && vdepool_alloc(&packets, sizeof(vde_pkt), POOL_GROWTH))
		goto out_unlock;
	cur = packets;
	while(cur) {
		if (cur->free_count == 0) {
			counter++;
			cur = cur->next;
			continue;
		}
		for (i = 0; i < cur->bmp_size; i++)
		{
			if (cur->bmp[i] != 0xFF) {
				for (j = 0; j < 8; j++) {
					if ((cur->bmp[i] & (1 << j)) == 0) {
						position = (i << 3) + j;
						set_bit(cur, position);
						p = (vde_pkt *)(cur->content + sizeof(vde_pkt) * position);
						p->_pool_pool = cur;
						p->_pool_pos = position;
						goto out_unlock;
					}
				}
			}
		}
		cur = cur->next;
	}
	if (position < 0) {
		/* Not enough space. Try to grow */
		if (vdepool_alloc(&packets, sizeof(vde_pkt), POOL_GROWTH) == 0) {
			pthread_mutex_unlock(&references_lock);
			return reference_alloc();
		}
	}
out_unlock:
	pthread_mutex_unlock(&references_lock);
	return p;
}

vde_pkt *vdepool_pkt_new(int data_size)
{
	int i, j;
	vde_pkt *p = NULL;
	int position = -1;
	struct vdepool *cur = pool;

	pthread_mutex_lock(&mempool_lock);
	if (!pool && vdepool_alloc(&pool, VDE_PACKET_SIZE, POOL_GROWTH))
		goto out_unlock;

	while(cur) {
		if (cur->free_count == 0) {
			cur = cur->next;
			continue;
		}
		for (i = 0; i < cur->bmp_size; i++)
		{
			if (cur->bmp[i] != 0xFF) {
				for (j = 0; j < 8; j++) {
					if ((cur->bmp[i] & (1 << j)) == 0) {
						position = (i << 3) + j;
						goto found;
					}
				}
			}
		}
		cur = cur->next;
	}
	if (position < 0) {
		/* Not enough space. Try to grow */
		if (vdepool_alloc(&pool, VDE_PACKET_SIZE, POOL_GROWTH) == 0) {
			pthread_mutex_unlock(&mempool_lock);
			return vdepool_pkt_new(data_size);
		} else
			goto out_unlock;
	}

found:
	if (position >= 0) {
		p = reference_alloc();
		if (!p)
			goto out_unlock;
		p->pkt = (vde_pkt_content *) (cur->content + VDE_PACKET_SIZE * position);
		p->pkt->_pool = cur;
		set_bit(cur, position);
		p->pkt->usage_count = 1;
		p->pkt->_pos = position;
  		p->hdr = (vde_hdr *)p->pkt->data;
  		p->head = p->pkt->data + sizeof(vde_hdr);
  		p->payload = p->head;
  		p->tail = p->pkt->data + data_size;
  		p->data_size = data_size;
	}
out_unlock:
	pthread_mutex_unlock(&mempool_lock);
	return p;
}

static void reference_free(vde_pkt *p)
{
	pthread_mutex_lock(&references_lock);
	unset_bit(p->_pool_pool, p->_pool_pos);
	pthread_mutex_unlock(&references_lock);
}

void vdepool_pkt_discard(vde_pkt *p)
{
	pthread_mutex_lock(&mempool_lock);
	p->pkt->usage_count--;
	if (p->pkt->usage_count < 1) {
		unset_bit(p->pkt->_pool, p->pkt->_pos);
	}
	/* In either case, this copy of the 
	packet is not needed any more. */
	reference_free(p);
	pthread_mutex_unlock(&mempool_lock);
}

vde_pkt *vdepool_pkt_compact_cpy(vde_pkt *orig)
{
	uint32_t stored_position;
	void *stored_pool;
	vde_pkt *p = reference_alloc();
	if (!p)
		return NULL;

	stored_pool = p->_pool_pool;
	stored_position = p->_pool_pos;
	memcpy(p, orig, sizeof(vde_pkt));
	p->_pool_pool = stored_pool;
	p->_pool_pos = stored_position;
	p->payload = p->head = p->pkt->data + sizeof(vde_hdr);
	p->tail = p->payload + p->data_size;
	p->pkt->usage_count++;
	return p;
}

vde_pkt *vdepool_pkt_cpy(vde_pkt *orig)
{
	vde_pkt *p = vdepool_pkt_compact_cpy(orig);
	if (p) {
		p->head = orig->head;
		p->payload = orig->payload;
		p->tail = orig->tail;
	}
	return p;
}

vde_pkt *vdepool_pkt_deepcpy(vde_pkt *orig)
{
	vde_pkt *p = vdepool_pkt_new(orig->data_size);
	if (!p)
		return NULL;
	p->head = orig->head;
	p->tail = orig->tail;
	memcpy(p->pkt, orig->pkt, VDE_PACKET_SIZE);
	return p;
}

vde_pkt *vdepool_pkt_compact_deepcpy(vde_pkt *orig)
{
	vde_pkt *p = vdepool_pkt_new(orig->data_size);
	if (!p)
		return NULL;
	p->head = orig->head;
	p->tail = orig->tail;
	memcpy(p->pkt->data, orig->pkt->data, VDE_PACKET_SIZE - sizeof(vde_pkt_content));
	return p;
}

/**
 * @brief Initialize vde packet fields.
 *
 * @param pkt The packet to initialize
 * @param data The size of preallocated memory
 * @param head The size of the space before payload
 * @param tail The size of the space after payload
 */
void vde_pkt_setprops(vde_pkt *pkt, unsigned int head, unsigned int tail)
{
  pkt->hdr = (vde_hdr *)pkt->pkt->data;
  pkt->head = pkt->pkt->data + sizeof(vde_hdr);
  pkt->payload = pkt->head + head;
  pkt->tail = pkt->pkt->data + pkt->data_size - tail;
}

/**
 * @brief Allocate and initialize a new vde_pkt
 *
 * @param payload_sz The size of the payload
 * @param head The size of the space before payload
 * @param tail The size of the space after payload
 *
 * @return The new packet on success, NULL on error (and errno is set
 * appropriately)
 */
vde_pkt *vde_pkt_new(unsigned int payload_sz, unsigned int head,
                                   unsigned int tail)
{
  unsigned int data_sz = sizeof(vde_hdr) + head + payload_sz + tail;
  unsigned int pkt_sz = sizeof(vde_pkt) + data_sz;
  vde_pkt *pkt = vdepool_pkt_new(pkt_sz);

  if (pkt == NULL) {
    errno = ENOMEM;
    return NULL;
  }
  if (head || tail)
    vde_pkt_setprops(pkt, head, tail);
  return pkt;
}

void mempool_init(void)
{
	pthread_mutex_init(&mempool_lock, NULL);
	pthread_mutex_init(&references_lock, NULL);

}

// When a packet is read from the network by a connection the payload always
// follows the header, so head size and tail size are zero.
// If a connection implementation does not handle generic vde data but specific
// payload types (e.g.: vde2-compatibile transport or tap transport) it will
// populate the header of a new packet.

// An engine/connection_manager can instruct the connection to pre-allocate
// additional memory around the payload for further elaboration:
//void vde_connection_set_pkt_properties(vde_connection *conn,
//                                       unsigned int head_sz,
//                                       unsigned int tail_sz);

// For further speedup certain implementations might want to define their own
// data structure to be used when specific properties are set by the engine.
// struct vde2_transport_pkt {
//   vde_pkt pkt;
//   char data[1540]; // sizeof(vde_hdr) +
//                    // 4 (head reserved for vlan tags) +
//                    // 1504 (frame eth+trailing)
// };
//
// Inside a connection handling read event:
// struct vde2_transport_pkt stack_pkt;
// vde_pkt *pkt;
// ...
// if(conn->head_sz == 4 && conn->tail_sz == 0) {
//   ... set fields as explained above ...
//   read(stack_pkt.pkt.payload, MAX_ETH_FRAME_SIZE);
//   *pkt = stack_pkt;
// } else {
//   ... alloc a new vde_pkt with the necessary data and read ...
// }
// ... set packet fields and pass it to the engine ...


