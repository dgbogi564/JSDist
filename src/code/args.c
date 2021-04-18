#include "args.h"
#include "errorh.h"

args_t* args_init() {
	args_t* args = hmalloc(sizeof(args_t));
	args->ext = NULL;
	args->dirq = args->fileq = NULL;
	return args;
}