#include "queue.h"
#include "errorh.h"
#include <stdlib.h>
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;


queue_t* queue_init() {
	queue_t *queue = hmalloc(sizeof(queue_t));
	hpthread_cond_init(&queue->write_ready, NULL);
	queue->size = queue->num_threads = queue->num_working = 0;
	queue->head = queue->rear = NULL;
	queue->lock = lock;
	return queue;
}

void enqueue(queue_t *queue, char *data) {
	node_t *node = node_init(data);
	hpthread_mutex_lock(&lock);

	if(queue->head != NULL) {
		queue->rear->next = node;
		queue->rear = node;
	} else queue->head = queue->rear = node;
	++queue->size;

	hpthread_mutex_unlock(&lock);
}


void* dequeue(queue_t *queue) {
	hpthread_mutex_lock(&lock);
	if(queue->head == NULL) {
		hpthread_mutex_unlock(&lock);
		return NULL;
	}

	node_t *node = queue->head;
	char *data = node->data;
	queue->head = queue->head->next;
	if(queue->head == queue->rear) queue->rear = NULL;
	--queue->size;

	hpthread_mutex_unlock(&lock);
	free(node);

	return data;
}

void free_queue(queue_t *queue) {
	hpthread_mutex_lock(&lock);

	node_t *head = queue->head, *temp;
	while(head != NULL) {
		temp = head->next;
		free(head);
		head = temp;
	}

	hpthread_mutex_unlock(&lock);
	hpthread_cond_destroy(&queue->write_ready);
	free(queue);
}