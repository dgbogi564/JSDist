#include "wfd.h"
#include "errorh.h"
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

wfdNode_t* wfdNode_init(char *word) {
	wfdNode_t *wfdNode = hmalloc(sizeof(wfdNode_t));
	wfdNode->occurrences = 1;
	wfdNode->freq = 0;
	wfdNode->word = word;
	wfdNode->next = NULL;
	return wfdNode;
}

wfd_t* wfd_init(char *file) {
	wfd_t *wfd = hmalloc(sizeof(wfd_t));
	wfd->size = wfd->total_occurrences = 0;
	wfd->file = file;
	wfd->head = NULL;
	wfd->next = NULL;
	return wfd;
}

wfdLL_t*  wfdLL_init() {
	wfdLL_t *wfdLinkedList = hmalloc(sizeof(wfdLL_t));
	wfdLinkedList->size = 0;
	wfdLinkedList->lock = lock;
	wfdLinkedList->head = NULL;
	return wfdLinkedList;
}

void add_wfdNode(wfd_t* wfd, char *word) {
	wfdNode_t *wfdNode = wfd->head;
	if(wfdNode == NULL) wfd->head = wfdNode_init(word);
	else {
		while(wfdNode->next != NULL) wfdNode = wfdNode->next;
		wfdNode->next = wfdNode_init(word);
	}
	++wfd->size;
}

void insert_wfd(wfdLL_t *wfdLinkedList, wfd_t *wfd) {
	hpthread_mutex_lock(&lock);

	wfd_t *wfd2 = wfdLinkedList->head;
	if(wfd2->head == NULL) wfdLinkedList->head = wfd;
	else {
		while(wfd2->next != NULL) wfd2 = wfd2->next;
		wfd2->next = wfd;
	}
	++wfdLinkedList->size;

	hpthread_mutex_unlock(&lock);
}

void calculate_freq(wfd_t *wfd) {
	wfdNode_t *wfdNode = wfd->head;
	while(wfdNode != NULL) {
		wfdNode->freq = (double)wfdNode->occurrences/(double)wfd->total_occurrences;
		wfdNode = wfdNode->next;
	}
}

void free_wfdLL(wfdLL_t *wfdLinkedList) {
	hpthread_mutex_lock(&lock);

	wfd_t *wfd = wfdLinkedList->head, *temp;
	wfdNode_t *wfdNode, *temp2;

	while(wfd != NULL) {
		while(wfdNode != NULL) {
			temp2 = wfdNode->next;
			free(wfdNode->word);
			free(wfdNode);
			wfdNode = temp2;
		}
		temp = wfd->next;
		free(wfd);
		wfd = temp;
	}

	hpthread_mutex_unlock(&lock);
}