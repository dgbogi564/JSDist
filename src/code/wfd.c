#include "wfd.h"

#include <stdio.h>
#include <stdlib.h>


wfdn_t* wfdn_init(char *word) {
	wfdn_t* wfdNode = (wfdn_t *)malloc(sizeof(wfdn_t));
	if(wfdNode == NULL) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}
	wfdNode->freq = -1;
	wfdNode->next = NULL;
	wfdNode->word = word;
	wfdNode->occurrences = 1;

	return wfdNode;
}

wfd_t* wfd_init(char *file) {
	wfd_t *wfd = malloc(sizeof(wfd_t));
	if(wfd == NULL) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}

	wfd->head = NULL;
	wfd->file = file;
	wfd->size = 0;

	return wfd;
}

wfdl_t* wfdl_init() {
	wfdl_t* wfdList = malloc(sizeof(wfdl_t));
    if(wfdList == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    wfdList->head = NULL;
    wfdList->size = 0;
	if(pthread_mutex_init(&wfdList->lock, NULL)) {
		perror("pthread_mutex_init");
		exit(EXIT_FAILURE);
	}

	return wfdList;
}

#include <string.h>
void insert_wfdn(wfd_t *wfd, char *word) {
	wfd->total_occurrences++;
	wfd->size++;

	if(wfd->head == NULL) wfd->head = wfdn_init(word);
	else {
		wfdn_t *wfdNode = wfd->head;
		wfdn_t *prev = NULL;
		int ret;
		while(wfdNode != NULL && (ret = strcmp(word, wfdNode->word)) > 0) {
			prev = wfdNode;
			wfdNode = wfdNode->next;
		}

		if(!ret) wfdNode->occurrences++;
		else {
			if(prev == NULL) {
				prev = wfdNode;
				wfd->head = wfdn_init(word);
				wfd->head->next = prev;
			} else {
				prev->next = wfdn_init(word);
				prev->next->next = wfdNode;
			}
		}
	}
}

void insert_wfd(wfdl_t *wfdList, wfd_t *wfd) {
	wfdList->size++;

	if(wfdList->head == NULL) wfdList->head = wfd;
	else {
		wfd_t *wfdNode = wfdList->head;
		while(wfdNode->next != NULL) wfdNode = wfdNode->next;
		wfdNode->next = wfd;
	}
}

void calculate_freq(wfd_t *wfd) {
	wfdn_t *wfdNode = wfd->head;
	while(wfdNode != NULL) {
		wfdNode->freq = (double)wfdNode->occurrences/(double)wfd->total_occurrences;
		wfdNode = wfdNode->next;
	}
}

void free_wfdl(wfdl_t *wfdList) {
	wfd_t *wfd = wfdList->head;
	wfd_t *temp;
	wfdn_t *temp2;
	while(wfd != NULL) {
		wfdn_t *wfdNode = wfd->head;
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

    if(pthread_mutex_destroy(&wfdList->lock)) {
        perror("pthread_mutex_destroy");
        exit(EXIT_FAILURE);
    }

	free(wfdList);
}

