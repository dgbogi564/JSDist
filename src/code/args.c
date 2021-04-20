#include "args.h"
#include "errorh.h"
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

args_t* args_init() {
	args_t* args = hmalloc(sizeof(args_t));
	args->ext = NULL;
	args->dirq = args->fileq = NULL;
	args->lock = lock;
	return args;
}