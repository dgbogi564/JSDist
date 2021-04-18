#ifndef P2_WFD_H
#define P2_WFD_H

typedef struct wfdNode {
	int occurrences;
	double freq;
	char *word;
	struct wfdNode *next;
} wfdn_t;

typedef struct wfd {
	char *file;
	int size, total_occurrences;
	wfdn_t *head;
	struct wfd *next;
} wfd_t;

#include <pthread.h>
typedef struct wfdList {
	int size;
	pthread_mutex_t lock;
    wfd_t *head;
} wfdl_t;

wfdn_t* wfdn_init(char *word);

wfd_t* wfd_init(char *file);

wfdl_t* wfdl_init();

void insert_wfdn(wfd_t *wfd, char *word);

void insert_wfd(wfdl_t *wfdList, wfd_t *wfd);

void calculate_freq(wfd_t *wfd);

void free_wfdl(wfdl_t *wfdList);

#endif //P2_WFD_H