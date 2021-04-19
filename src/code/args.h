#ifndef P2_ARGS_H
#define P2_ARGS_H

#include "queue.h"
#include "wfd.h"

typedef struct args {
	char *ext;
	int d, f, a, thread_id;
	queue_t *dirq, *fileq;
	wfdLL_t *wfd_repo;
    pthread_mutex_t lock;
} args_t;

args_t* args_init();

#endif //P2_ARGS_H
