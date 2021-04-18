#include "lock.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <pthread.h>

void lock(pthread_mutex_t *lock) {
	if(pthread_mutex_lock(lock) == EINVAL) {
		perror("pthread_mutex_lock");
		exit(EXIT_FAILURE);
	}
}

void unlock(pthread_mutex_t *lock) {
	if(pthread_mutex_unlock(lock) == EINVAL) {
		perror("pthread_mutex_lock");
		exit(EXIT_FAILURE);
	}
}