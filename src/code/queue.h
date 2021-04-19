#ifndef P2_QUEUE_H
#define P2_QUEUE_H

#include "node.h"

#include <pthread.h>
typedef struct queue {
	int size, num_threads, num_working;
	struct node *head, *rear;
	pthread_mutex_t lock;
	pthread_cond_t write_ready;
} queue_t;


queue_t* queue_init();

void enqueue(queue_t *queue, char *data);

void* dequeue(queue_t *queue);

void free_queue(queue_t *queue);

#endif //P2_QUEUE_H
