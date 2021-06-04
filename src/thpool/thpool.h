//////////////////////////////////////////////////////////////////////
//	filename: 	thpool.h
//
//	purpose:	Creates synchronized thread pools and allows users to
//          	enqueue 'jobs,' a struct that takes function pointers
//          	and their arguments, into a list; one of the open
//          	threads takes this struct and calls the function with
//          	the given arguments.
//
//  important:  All functions are assumed to return -1 on failure
//              and 0 on success.
//
/////////////////////////////////////////////////////////////////////


#ifndef THPOOL_H
#define THPOOL_H


#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdbool.h>

#include <pthread.h>



/* ========================== STRUCTURES ============================ */


/* Job */
typedef struct job_ {
    int8_t (*function)(void *args);     /* Pointer to function to be run                            */
    void *args;                         /* Args given to function to run                            */
    struct job_ *next;                  /* Pointer to next job                                      */
} job_;


/* Job list */
typedef struct joblist_ {
    uint32_t size;                      /* Size of job list                                         */
    job_ *front;                        /* Pointer to first job in list                             */
    job_ *rear;                         /* Pointer to last job in the list                          */
    pthread_mutex_t lock;               /* Job list thread locker                                   */
} joblist_;


/* Thread */
typedef struct thread_ {
    uint16_t ID;                        /* Thread ID                                                */
    pthread_t pthread;                  /* Pointer to the thread                                    */
    char *name;                         /* Name of the thread                                       */
    struct thpool_ *thpool;             /* Pointer to thread pool                                   */
} thread_;


/* Thread pool */
typedef struct thpool_ {
    volatile bool keep_alive;           /* Keeps thread pool alive regardless of status             */
    volatile bool suspend;              /* Suspends threads                                         */

    uint8_t num_threads;                /* Number of threads in the pool                            */
    uint8_t num_threads_working;        /* Number of threads working                                */

    thread_ **thread;                   /* Array of thread pointers                                 */
    joblist_ *joblist;                  /* Job list                                                 */

    pthread_barrier_t barrier;          /* Not needed by thpool — helps users synchronize threads   */
    pthread_cond_t resume;              /* Signals threads to resume work                           */
    pthread_mutex_t lock;               /* Thread lock                                              */
} thpool_;





/* ========================== PROTOTYPES ============================ */


/* Job list */
joblist_* joblist_init();
job_*     joblist_pop(joblist_ *joblist);


/* Threads */
thread_* thread_init(thpool_ *thpool, char *phase,  uint16_t ID);
int8_t   thread_do(thread_ *thread);


/* Thread Pool */
thpool_* thpool_init(uint8_t num_threads, char *phase);
int8_t   thpool_wait_and_destroy(thpool_ *thpool);
int8_t   thpool_add_work(thpool_ *thpool, int8_t (*function)(void*), void *args);






#endif //THPOOL_H
