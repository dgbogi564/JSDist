#ifndef P2_LLNODE_H
#define P2_LLNODE_H

#include <pthread.h>
#include <stdlib.h>
#include "compare.h"

typedef struct wfdNode {
	int occurrences;
	double freq;
	char *word;
	struct wfdNode *next;
} wfdNode_t;

typedef struct wfd {
	int size, total_occurrences;
	char *file;
	wfdNode_t *head;
	struct wfd *next;
} wfd_t;

typedef struct wfdLinkedList {
	int size;
	wfd_t *head;
	pthread_mutex_t lock;
} wfdLL_t;


wfdNode_t* wfdNode_init(char *word);

wfd_t* wfd_init(char *file);

wfdLL_t*  wfdLL_init();

wfdNode_t* add_wfdNode(wfd_t* wfd, char *word);

void insert_wfd(wfdLL_t *wfdLinkedList, wfd_t *wfd);

void calculate_freq(wfd_t *wfd);

void free_wfdNode(wfdNode_t *wfdNode);
void free_wfd(wfd_t *wfd);
void free_wfdLL(wfdLL_t *wfdLinkedList);

#endif //P2_LLNODE_H
