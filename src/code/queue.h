#ifndef P2_QUEUE_H
#define P2_QUEUE_H


typedef struct node {
	char *path;
	struct node *next;
} node_t;

#include <pthread.h>
typedef struct queue {
	int size;
	int num_threads;
	int threads_working;
	node_t *head, *rear;
	pthread_mutex_t lock;
	pthread_cond_t write_ready;
} queue_t;

node_t* node_init(char *path);

queue_t* queue_init();

void enqueue(queue_t *queue, char *path);

char* dequeue(queue_t *queue);

void free_queue(queue_t *queue);

#endif //P2_QUEUE_H