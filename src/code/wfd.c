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

wfdNode_t* add_wfdNode(wfd_t* wfd, char *word) {
	wfdNode_t *wfdNode = NULL;

	if(wfd->head == NULL) {
		wfd->head = wfdNode_init(word);
		wfd->size = 1;
		wfdNode = wfd->head;
	} else {
		int ret;
		wfdNode_t *prev = NULL;
		wfdNode = wfd->head;
		while(wfdNode != NULL && 0 < (ret = strcmp(word, wfdNode->word))) {
			prev = wfdNode;
			wfdNode = wfdNode->next;
		}

		if(ret) {
			if(prev == NULL) {
				prev = wfdNode;
				wfd->head = wfdNode = wfdNode_init(word);
				wfd->head->next = prev;
			} else {
				prev = prev->next = wfdNode_init(word);
				prev->next = wfdNode;
				wfdNode = prev;
			}
		} else {
			++wfdNode->occurrences;
			wfdNode = NULL;
		}
	}

	++wfd->total_occurrences;
	return wfdNode;
}

void insert_wfd(wfdLL_t *wfdLinkedList, wfd_t *wfd) {
	hpthread_mutex_lock(&lock);
	wfd_t *wfdTemp;

	if((wfdTemp = wfdLinkedList->head) == NULL) {
		wfdLinkedList->head = wfd;
	} else {
		while(wfdTemp->next != NULL) wfdTemp = wfdTemp->next;
		wfdTemp->next = wfd;
	}
	++wfdLinkedList->size;

	hpthread_mutex_unlock(&lock);
}

void calculate_freq(wfd_t *wfd) {
	wfdNode_t *wfdNode = wfd->head;
	while(wfdNode != NULL) {
		wfdNode->freq = (double)wfdNode->occurrences/(double)wfd->total_occurrences;
		if(DEBUG) printf("%-18s %-18d %lf\n", wfdNode->word, wfdNode->occurrences, wfdNode->freq);
		wfdNode = wfdNode->next;
	}
}

void free_wfdNode(wfdNode_t *wfdNode) {
	wfdNode_t *temp;
	while(wfdNode != NULL) {
		temp = wfdNode->next;
		free(wfdNode->word);
		free(wfdNode);
		wfdNode = temp;
	}
}
void free_wfd(wfd_t *wfd) {
	free_wfdNode(wfd->head);
	free(wfd);
}
void free_wfdLL(wfdLL_t *wfdLinkedList) {
	hpthread_mutex_lock(&lock);
	wfd_t *wfd = wfdLinkedList->head;
	while(wfd != NULL) {
		free_wfd(wfd);
		wfd = wfd->next;
	}
	hpthread_mutex_unlock(&lock);
}
