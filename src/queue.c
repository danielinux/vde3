/* VDE_ROUTER (C) 2007:2011 Daniele Lacamera
 *
 * Licensed under the GPLv2
 *
 */
#include <vde3/queue.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>

void enqueue(queue *q, vde_pkt *b)
{
	pthread_mutex_lock(&q->lock);

	if (!q->may_enqueue(q, b)) {
		vdepool_pkt_discard(b);
		pthread_mutex_unlock(&q->lock);
		return;
	}

	b->next = NULL;
	if (!q->head) {
		q->head = b;
		q->tail = b;
	} else {
		q->tail->next = b;
		q->tail = b;
	}
	q->size += b->data_size;
	q->n++;
	pthread_mutex_unlock(&q->lock);
	sem_post(&q->semaphore);
}

vde_pkt *dequeue(queue *q)
{
	vde_pkt *ret = NULL;
	sem_wait(&q->semaphore);
	pthread_mutex_lock(&q->lock);
	if (q->n) {
		ret = q->head;
		q->head = ret->next;
		q->n--;
		q->size -= ret->data_size;
		if (q->n == 0) {
			q->tail = NULL;
			q->head = NULL;
		}
	}
	pthread_mutex_unlock(&q->lock);
	return ret;
}

/* Unlimited policy */
int qunlimited_may_enqueue(queue *q, vde_pkt *b)
{
	return 1;
}


void qunlimited_setup(queue *q)
{
	pthread_mutex_lock(&q->lock);
	if (q->policy == QPOLICY_TOKEN) {
		//	vder_timed_dequeue_del(q);
		/* TODO: implement timed dequeue */
		assert(1==0);
	}
	q->policy = QPOLICY_UNLIMITED;
	q->may_enqueue = qunlimited_may_enqueue;
	pthread_mutex_unlock(&q->lock);
}


/* Fifo policy */
int qfifo_may_enqueue(queue *q, vde_pkt *b)
{
	if (q->policy_opt.fifo.limit > q->size)
		return 1;
	else {
		q->policy_opt.fifo.stats_drop++;
		return 0;
	}
}


void qfifo_setup(queue *q, uint32_t limit)
{
	pthread_mutex_lock(&q->lock);
	if (q->policy == QPOLICY_TOKEN) {
		//vder_timed_dequeue_del(q);
		/* TODO: implement timed dequeue */
		assert(1 == 0);
	}
	q->policy = QPOLICY_FIFO;
	q->policy_opt.fifo.limit = limit;
	q->policy_opt.fifo.stats_drop = 0;
	q->may_enqueue = qfifo_may_enqueue;
	pthread_mutex_unlock(&q->lock);
}

/* Random early detection */
int qred_may_enqueue(queue *q, vde_pkt *b)
{
	double red_probability;
	if (q->policy_opt.red.min > q->size) {
		return 1;
	} else if (q->policy_opt.red.max > q->size) {
		red_probability = q->policy_opt.red.P *
				((double)q->size - (double)q->policy_opt.red.min /
				((double)q->policy_opt.red.max - (double)q->policy_opt.red.min));
	} else if (q->policy_opt.red.limit > q->size) {
		red_probability = q->policy_opt.red.P;
	} else {
		q->policy_opt.red.stats_drop++;
		return 0;
	}
	if (drand48() < red_probability) {
		q->policy_opt.red.stats_probability_drop++;
		return 0;
	}
	return 1;
}


void qred_setup(queue *q, uint32_t min, uint32_t max, double P, uint32_t limit)
{
	pthread_mutex_lock(&q->lock);
	if (q->policy == QPOLICY_TOKEN) {
		/* TODO: implement timed dequeue */
		//vder_timed_dequeue_del(q);
		assert(1==0);
	}
	q->policy = QPOLICY_RED;
	q->policy_opt.red.min = min;
	q->policy_opt.red.max = max;
	q->policy_opt.red.P = P;
	q->policy_opt.red.limit = limit;
	q->policy_opt.red.stats_drop = 0;
	q->policy_opt.red.stats_probability_drop = 0;
	q->may_enqueue = qred_may_enqueue;
	pthread_mutex_unlock(&q->lock);
}

#define IDEAL_PACKET_SIZE 1500

int qtoken_may_enqueue(queue *q, vde_pkt *b)
{
	if (q->policy_opt.token.limit > q->size)
		return 1;
	else {
		q->policy_opt.token.stats_drop++;
		return 0;
	}
}

void qtoken_setup(queue *q, uint32_t bitrate, uint32_t limit)
{
	pthread_mutex_lock(&q->lock);
	q->policy_opt.token.interval = (1000000 * IDEAL_PACKET_SIZE) / ((bitrate >> 3));
	q->policy_opt.token.limit = limit;
	q->policy_opt.token.stats_drop = 0U;
	if (q->policy == QPOLICY_TOKEN) {
		//vder_timed_dequeue_del(q);
		/* TODO: implement timed dequeue */
		assert(1==0);
	}
	q->policy = QPOLICY_TOKEN;
	//vder_timed_dequeue_add(q, q->policy_opt.token.interval);
	q->may_enqueue = qtoken_may_enqueue;
	pthread_mutex_unlock(&q->lock);
}

void queue_init(queue *q)
{
    pthread_mutex_init(&q->lock, NULL);
    sem_init(&q->semaphore, 0, 0);
    memset(q, 0, sizeof(queue));
    qunlimited_setup(q);
}

