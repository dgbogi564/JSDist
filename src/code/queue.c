#include "queue.h"

#include <stdlib.h>
#include <stdio.h>

node_t* node_init(char *path) {
	node_t *node = malloc(sizeof(node_t));
	if(node == NULL) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}

	node->path = path;
	node->next = NULL;

	return node;
}

queue_t* queue_init() {
	queue_t *queue = (queue_t *)malloc(sizeof(queue_t));
	if(queue == NULL) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}

	if(pthread_mutex_init(&queue->lock, NULL)) {
		perror("pthread_mutex_init");
		exit(EXIT_FAILURE);
	}

	if(pthread_cond_init(&queue->write_ready, NULL)) {
		perror("pthread_cond_init");
		exit(EXIT_FAILURE);
	}



	queue->size = 0;
    queue->num_threads = 1;
	queue->threads_working = 1;
	queue->head = NULL;
	queue->rear = NULL;

	return queue;
}

void enqueue(queue_t *queue, char *path) {
	node_t *node = node_init(path);
	if(queue->head != NULL) {
		queue->rear->next = node;
		queue->rear = node;
	} else queue->head = queue->rear = node;

	queue->size++;
}

char* dequeue(queue_t *queue) {
	if(queue->head == NULL) return NULL;
	queue->size--;

    char *path = queue->head->path;
	node_t *node = queue->head->next;
	if(queue->head == queue->rear) queue->rear = NULL;
	free(queue->head);
	queue->head = node;

	return path;
}

void free_queue(queue_t *queue) {
	node_t *head = queue->head;
	node_t *temp;
	while(head != NULL) {
		temp = head->next;
		free(head);
		head = temp;
	}

	if(pthread_cond_destroy(&queue->write_ready)) {
		perror("pthread_cond_destroy");
		exit(EXIT_FAILURE);
	}

    if(pthread_mutex_destroy(&queue->lock)) {
        perror("pthread_mutex_destroy");
        exit(EXIT_FAILURE);
    }

	free(queue);
}