#ifndef P2_COMPARE_H
#define P2_COMPARE_H

#include "queue.h"
#include "wfd.h"
#include "jsd.h"

typedef struct args {
	char *ext;
	char *path;
	int num_threads;
	int thread_id;
	queue_t *dirq;
	queue_t *fileq;
	wfdl_t *wfd_repo;
	jsdl_t *jsd_repo;
} args_t;

#ifndef BUFSIZE
#define BUFSIZE 256
#endif

#ifndef DEBUG
#define DEBUG 1
#endif

#endif //P2_COMPARE_H
