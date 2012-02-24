/* VDE_ROUTER (C) 2007:2011 Daniele Lacamera
 *
 * Licensed under the GPLv2
 *
 */
#ifndef __VDER_QUEUE
#define __VDER_QUEUE
#include <stdint.h>
#include <semaphore.h>
#include "include/vde3/packet.h"

enum queue_policy_e {
    QPOLICY_UNLIMITED = 0,
    QPOLICY_FIFO,
    QPOLICY_RED,
    QPOLICY_TOKEN
};


/* Queue */
struct queue {
	uint32_t n; /*< Number of packets */
	uint32_t size; /*< this is in bytes */
	pthread_mutex_t lock;
	sem_t semaphore;
	vde_pkt *head;
	vde_pkt *tail;
	uint8_t type;
	sem_t *prio_semaphore;

	enum queue_policy_e policy;
	int (*may_enqueue)(struct queue *q, vde_pkt *vb);
	int (*may_dequeue)(struct queue *q);
	union policy_opt_e {
		struct {
			uint32_t limit;
			uint32_t stats_drop;
		} fifo;
		struct {
			uint32_t min;
			uint32_t max;
			double P;
			uint32_t limit;
			uint32_t stats_drop;
			uint32_t stats_probability_drop;
		} red;
		struct {
			uint32_t limit;
			uint32_t stats_drop;
			unsigned long long interval;
		} token;
	}policy_opt;
};

typedef struct queue queue;

void queue_init(queue *q);

void enqueue(queue *q, vde_pkt *b);
vde_pkt *dequeue(queue *q);

void qunlimited_setup(queue *q);
void qfifo_setup(queue *q, uint32_t limit);
void qred_setup(queue *q, uint32_t min, uint32_t max, double P, uint32_t limit);
void qtoken_setup(queue *q, uint32_t bitrate, uint32_t limit);


int qunlimited_may_enqueue(queue *q, vde_pkt *b);
int qunlimited_may_dequeue(queue *q);

int qfifo_may_enqueue(queue *q, vde_pkt *b);
int qfifo_may_dequeue(queue *q);

int qred_may_enqueue(queue *q, vde_pkt *b);
int qred_may_dequeue(queue *q);


#endif
