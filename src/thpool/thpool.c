#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <pthread.h>
#include <sys/prctl.h>
#include <unistd.h>

#include "thpool.h"


#ifdef THPOOL_DEBUG
    #define THPOOL_DEBUG 1
#else
    #define THPOOL_DEBUG 0
#endif

#if defined(DISABLE_PRINT) == 0 || THPOOL_DEBUG
    #define err(format, ...) fprintf(stderr, format, ##__VA_ARGS__)
#else
    #define err(str)
#endif



/* ============================= JOBS =============================== */


joblist_* joblist_init() {

    joblist_ *joblist = malloc(sizeof(joblist_));
    if (joblist == NULL) {
        err("joblist_init(): Failed to initialize joblist\n");
        return NULL;
    }

    joblist->size = 0;
    joblist->front = NULL;
    joblist->rear = NULL;

    if (pthread_mutex_init(&joblist->lock, NULL)) {
        err("joblist_init(): Failed to initialize mutex\n");
        free(joblist);
        return NULL;
    }


    return joblist;
}

void joblist_destroy(joblist_ *joblist) {

    job_ *job = joblist->front;
    job_ *temp;

    while (job != NULL) {
        temp = job->next;
        free(job);
        job = temp;
    }

    pthread_mutex_destroy(&joblist->lock);


    free(joblist);
}

job_ *joblist_pop(joblist_ *joblist) {

    job_ *popped;

    pthread_mutex_lock(&joblist->lock);

        joblist->size--;

        popped = joblist->front;
        joblist->front = popped->next;
		if(joblist->front == NULL) {
			joblist->rear = NULL;
		}

    pthread_mutex_unlock(&joblist->lock);


    return popped;
}





/* ============================ THREAD ============================== */


thread_* thread_init(thpool_ *thpool, char *name,  uint16_t ID) {

    thread_ *thread = malloc(sizeof(thread_));
    if (thread == NULL) {
        err("Failed to initialize thread\n");
        return NULL;
    }

    thread->ID = ID;
    thread->name = name;
    thread->thpool = thpool;

    return thread;
}


int8_t thread_do(thread_ *thread) {

    int8_t return_value = 0;


    /* Set thread name */
    char thread_name[32] = {0};
    snprintf(thread_name, 31, "%s-%d", thread->name, thread->ID);
    prctl(PR_SET_NAME, thread_name);


    thpool_ *thpool = thread->thpool;
    joblist_ *joblist = thpool->joblist;

    while (thpool->keep_alive) {

        pthread_mutex_lock(&thpool->lock);
            thpool->num_threads_working++;
        pthread_mutex_unlock(&thpool->lock);

        /* Do jobs */
        while (joblist->size) {
            job_ *job = joblist_pop(joblist);
            if ((return_value = job->function(job->args)) != 0) {
                return_value = -1;
            }
            free(job);
        }


        pthread_mutex_lock(&thpool->lock);

	        /* Suspend thread if all work is finished */
            if(!joblist->size) {
	            thpool->suspend = 1;
	            while(thpool->suspend != 0) {
		            pthread_cond_wait(&thpool->resume, &thpool->lock);
	            }
            }

	    pthread_mutex_unlock(&thpool->lock);
    }


	pthread_mutex_lock(&thpool->lock);
        thpool->num_threads--;
	pthread_mutex_unlock(&thpool->lock);

    return return_value;
}





/* ========================== THREAD POOL =========================== */


thpool_* thpool_init(uint8_t num_threads, char *phase) {

    /* Initialize thread pool */
    thpool_ *thpool = calloc(1, sizeof(thpool_));
    if (thpool == NULL) {
        err("thpool_init(): Failed to initialize thpool\n");
        return NULL;
    }

    thpool->keep_alive = 1;
    thpool->suspend = 0;
    thpool->num_threads = num_threads;
    thpool->num_threads_working = 0;


    /* Initialize barrier */
    if (pthread_barrier_init(&thpool->barrier, NULL, num_threads)) {
        err("thpool_init(): Failed to initialize barrier\n");
        thpool_wait_and_destroy(thpool);
        return NULL;
    }

    /* Initialize condition */
    if (pthread_cond_init(&thpool->resume, NULL)) {
        err("thpool_init(): Failed to initialize condition\n");
        thpool_wait_and_destroy(thpool);
        return NULL;
    }

    /* Initialize mutex */
    if (pthread_mutex_init(&thpool->lock, NULL)) {
        err("thpool_init(): Failed to initialize mutex\n");
        thpool_wait_and_destroy(thpool);
        return NULL;
    }


	/* Initialize job list */
	thpool->joblist = joblist_init();
	if (thpool->joblist == NULL) {
		thpool_wait_and_destroy(thpool);
		return NULL;
	}


	/* Initialize threads */
	thpool->thread = malloc(sizeof(thread_ *));
	if (thpool->thread == NULL) {
		err("thpool_init(): Failed to initialize thread array\n");
		thpool_wait_and_destroy(thpool);
		return NULL;
	}

	for (uint8_t i = 0; i < num_threads; ++i) {

		thpool->thread[i] = thread_init(thpool, phase, i);
		if (thpool->thread[i] == NULL) {
			thpool_wait_and_destroy(thpool);
			return NULL;
		}

		pthread_create(&thpool->thread[i]->pthread, NULL, (void *)thread_do, (void *)thpool->thread[i]);

		thpool->num_threads++;
	}

	pthread_mutex_lock(&thpool->lock);
		thpool->num_threads--;
	pthread_mutex_unlock(&thpool->lock);

    return thpool;
}


int8_t thpool_wait_and_destroy(thpool_ *thpool) {

    int8_t ERROR = 0;


    const struct timespec duration = {0, 1000};

    /* Wait for all work and threads to finish */
	while(thpool->num_threads_working) {
		thpool->suspend = 0;
		pthread_cond_signal(&thpool->resume);
		clock_nanosleep(CLOCK_REALTIME, 0, &duration, NULL);
	}

	thpool->keep_alive = 0;
	while(thpool->num_threads) {
		thpool->suspend = 0;
		pthread_cond_broadcast(&thpool->resume);
		clock_nanosleep(CLOCK_REALTIME, 0, &duration, NULL);
	}


    /* Join all threads and check if they closed with an error */
    for (uint32_t i = 0; i < thpool->num_threads; ++i) {
        void *return_value;
        pthread_join(thpool->thread[i]->pthread, &return_value);

        if((uintptr_t)return_value == -1) {
            ERROR = -1;
        }
    }


    /* Destroy mutex, barrier, and conditions */
    pthread_mutex_destroy(&thpool->lock);
    pthread_barrier_destroy(&thpool->barrier);
    pthread_cond_destroy(&thpool->resume);


    /* Destroy job list */
    joblist_destroy(thpool->joblist);


    /* Destroy threads */
    for (uint8_t i = 0; i < thpool->num_threads; ++i) {
        free(thpool->thread[i]);
    }

    free(thpool->thread);
    free(thpool);


    if(ERROR) {
        return -1;
    }
    return 0;
}


int8_t thpool_add_work(thpool_ *thpool, int8_t (*function)(void*), void *args) {

    joblist_ *joblist = thpool->joblist;

    /* Initialize job */
    job_ *job = malloc(sizeof(job_));
    if (job == NULL) {
        err("joblist_init(): Failed to initialize job\n");
        return -1;
    }

    job->function = function;
    job->args = args;
    job->next = NULL;


    /* Push job into the job list */
    pthread_mutex_lock(&joblist->lock);

        joblist->size++;
        if (joblist->front == NULL) {
            joblist->front = job;
            joblist->rear = job;
        } else {
            joblist->rear->next = job;
            joblist->rear = job;
        }

    pthread_mutex_unlock(&joblist->lock);


    /* Signal a thread to do work */
    pthread_mutex_lock(&thpool->lock);
        thpool->suspend = 0;
        pthread_cond_signal(&thpool->resume);
	pthread_mutex_unlock(&thpool->lock);

    return 0;
}