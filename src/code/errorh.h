#ifndef P2_ERRORH_H
#define P2_ERRORH_H

#include <stdio.h>
#include <errno.h>

#include <sys/stat.h>

void* hmalloc(size_t size);
int hstat(const char* file, struct stat *buf);
#include <unistd.h>
int hread(int fd, void* buf, size_t nbytes);

#include <pthread.h>
void hpthread_create(pthread_t *thread, const pthread_attr_t *attr, void*(*start_routine) (void *), void *arg);
void hpthread_join(pthread_t thread, void **retval);
void hpthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr);
void hpthread_mutex_destroy(pthread_mutex_t *mutex);
void hpthread_mutex_lock(pthread_mutex_t *mutex);
void hpthread_mutex_unlock(pthread_mutex_t *mutex);
void hpthread_cond_init(pthread_cond_t *cond, pthread_condattr_t* cond_attr);
void hpthread_cond_destroy(pthread_cond_t *cond);
void hpthread_cond_broadcast(pthread_cond_t *cond);

#include <string.h>
int hstrtol(const char *nptr, char **endptr, int base);

int isTYPE(struct stat info);

#endif //P2_ERRORH_H
