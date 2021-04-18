#ifndef P2_LOCK_H
#define P2_LOCK_H
#include <pthread.h>

void lock(pthread_mutex_t *lock);
void unlock(pthread_mutex_t *lock);

#endif //P2_LOCK_H
