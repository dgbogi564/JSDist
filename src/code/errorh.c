#include "errorh.h"
#include <stdlib.h>


void* hmalloc(size_t size) {
	void* ptr = malloc(size);
	if(ptr == NULL) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}
	return ptr;
}

int hstat(const char* file, struct stat *buf) {
	if(stat(file, buf) == -1) {
		perror("stat");
		return 1;
	}
	return 0;
}
int isTYPE(struct stat info) {
	if(S_ISREG(info.st_mode)) return 0;	// Is file.
	if(S_ISDIR(info.st_mode)) return 1;	// Is directory.
	return -1;                                   // Is neither.
}
int hread(int fd, void* buf, size_t nbytes) {
	int ret;
	errno = 0;
	if((ret = (int)read(fd, buf, nbytes)) == -1) perror("read");
	return ret;
}
struct dirent* hreaddir(DIR *dirp, int *FILE_ERROR) {
	struct dirent *entry;

	errno = 0;
	if((entry = readdir(dirp)) == NULL && errno) {
		perror("readdir");
		*FILE_ERROR = 1;
	}

	return entry;
}
DIR *hopendir(const char *name, int *FILE_ERROR) {
	DIR *ret;
	if((ret = opendir(name))) {
		*FILE_ERROR = 1;
	}
	return ret;
}


void hpthread_create(pthread_t *thread, const pthread_attr_t *attr, void*(*start_routine) (void *), void *arg) {
	if(pthread_create(thread, attr, start_routine, arg)) {
		perror("pthread_create");
		exit(EXIT_FAILURE);
	}
}
void hpthread_join(pthread_t thread, void **retval) {
	if(pthread_join(thread, retval)) {
		perror("pthread_create");
		exit(EXIT_FAILURE);
	}
}
void hpthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr) {
	if(pthread_mutex_init(mutex, attr)) {
		perror("pthread_mutex_init");
		exit(EXIT_FAILURE);
	}
}
void hpthread_mutex_destroy(pthread_mutex_t *mutex) {
	if(pthread_mutex_destroy(mutex)) {
		perror("pthread_mutex_destroy");
		exit(EXIT_FAILURE);
	}
}
void hpthread_mutex_lock(pthread_mutex_t *mutex) {
	if(pthread_mutex_lock(mutex)) {
		perror("pthread_mutex_lock");
		exit(EXIT_FAILURE);
	}
}
void hpthread_mutex_unlock(pthread_mutex_t *mutex) {
	if(pthread_mutex_unlock(mutex)) {
		perror("pthread_mutex_unlock");
		exit(EXIT_FAILURE);
	}
}
void hpthread_cond_init(pthread_cond_t *restrict cond, const pthread_condattr_t *restrict attr) {
	if(pthread_cond_init(cond, attr)) {
		perror("pthread_cond_init");
		exit(EXIT_FAILURE);
	}
}
void hpthread_cond_destroy(pthread_cond_t *cond) {
	if(pthread_cond_destroy(cond)) {
		perror("pthread_cond_destroy");
		exit(EXIT_FAILURE);
	}
}
void hpthread_cond_broadcast(pthread_cond_t *cond) {
	if(pthread_cond_broadcast(cond)) {
		perror("pthread_cond_broadcast");
		exit(EXIT_FAILURE);
	}
}
void hpthread_cond_timedwait(pthread_cond_t *restrict cond, pthread_mutex_t *restrict mutex, const struct timespec *restrict abstime) {
	if(pthread_cond_timedwait(cond, mutex, abstime)) {
		if(VERBOSE) perror("pthread_cond_timedwait");
		//exit(EXIT_FAILURE);
	} // Doesn't seem like there's any errors worth checking here.
}
void hclock_gettime(clockid_t clk_id, struct timespec *tp) {
	if(clock_gettime(clk_id, tp)) {
		perror("clock_gettime");
		exit(EXIT_FAILURE);
	}
}

int hstrtol(const char *nptr, char **endptr, int base) {
	errno = 0;
	int ret;
	if(errno || (ret = (int)strtol(nptr, endptr, base)) < 0) {
		fprintf(stderr, "Incorrect syntax.\n");
		exit(EXIT_FAILURE);
	}
	return ret;
}