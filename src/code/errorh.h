#ifndef P2_ERRORH_H
#define P2_ERRORH_H

#include <stdio.h>
#include <errno.h>


void* hmalloc(size_t size);

#include <sys/stat.h>
int hstat(const char* file, struct stat *buf);
int isTYPE(struct stat info);
#include <unistd.h>
int hread(int fd, void* buf, size_t nbytes);
#include <dirent.h>
struct dirent* hreaddir(DIR *dirp, int *FILE_ERROR);
DIR *hopendir(const char *name, int *FILE_ERROR);

#include <pthread.h>
void hpthread_create(pthread_t *restrict thread, const pthread_attr_t *restrict attr, void *(*start_routine)(void *), void *restrict arg);
void hpthread_join(pthread_t thread, void **retval);
void hpthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr);
void hpthread_mutex_destroy(pthread_mutex_t *mutex);
void hpthread_mutex_lock(pthread_mutex_t *mutex);
void hpthread_mutex_unlock(pthread_mutex_t *mutex);
void hpthread_cond_init(pthread_cond_t *restrict cond, const pthread_condattr_t *restrict attr);
void hpthread_cond_destroy(pthread_cond_t *cond);
void hpthread_cond_broadcast(pthread_cond_t *cond);
void hpthread_cond_timedwait(pthread_cond_t *restrict cond, pthread_mutex_t *restrict mutex, const struct timespec *restrict abstime);
#include <bits/types/clockid_t.h>
void hclock_gettime(clockid_t clk_id, struct timespec *tp);

#include <string.h>
int hstrtol(const char *nptr, char **endptr, int base);

#endif //P2_ERRORH_H
