#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <sys/prctl.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <math.h>
#include "compare.h"

/* ============================ STRUCTURES ============================== */


/* Boolean */
typedef enum BOOL {
    TRUE, FALSE
} boolean_;

/* Thread */
typedef struct thread_ {
    int id;
    char *name;
    pthread_t pthread;
    struct thpool_ *thpool;
} thread_;

/* Thread Pool */
typedef struct thpool_ {
    boolean_ threads_keepalive;
    boolean_ manager_ready;
    int num_threads;
    int num_threads_alive;
    int num_threads_working;
    thread_** threads;
    pthread_mutex_t lock;
    pthread_cond_t threads_all_idle;
} thpool_;

/* Node */
typedef struct node_ {
    char *path;
    struct node_ *next;
} node_;

/* File/directory queue */
typedef struct queue_ {
    int size;
    node_ *head;
    node_ *rear;
    pthread_mutex_t lock;
} queue_;

/* Word frequency distance node */
typedef struct wfdNode_ {
    int occurrences;
    double freq;
    char *word;
    struct wfdNode_ *next;
} wfdNode_;

/* Word frequency distance queue */
typedef struct wfdQueue_ {
    char *file;
    int total_occurrences;
    wfdNode_ *head;
    struct wfdQueue_ *next;
}wfdQueue_;

/* Word frequency distance double queue */
typedef struct wfdDoubleQueue_ {
    int size;
    wfdQueue_ *head;
    pthread_mutex_t lock;
} wfdDoubleQueue_;

/* Arguments */
typedef struct args_ {
    char* ext;
    queue_* fileq;
    queue_* dirq;
    thread_* thread;
    thpool_ *dPool;
    thpool_ *fPool;
    thpool_ *aPool;
    wfdDoubleQueue_ *wfdDoubleQueue;
} args_;


/* ============================ PROTOTYPES ============================== */


struct args_* args_init();

struct node_*  node_init(char *path);
struct queue_* queue_init();
void queue_push(queue_ *queue_p, char *path);
char* queue_pull(queue_ *queue_p);
void queue_destroy(queue_* queue_p);

wfdNode_* wfdNode_init(char *word);
wfdQueue_* wfdQueue_init(char *file);
wfdDoubleQueue_* wfdDoubleQueue_init();
wfdNode_* wfdQueue_push(wfdQueue_* wfdQueue, char *word);
void wfdDoubleQueue_push(wfdDoubleQueue_ *wfdDoubleQueue, wfdQueue_ *wfdQueue);
wfdQueue_* wfdDoubleQueue_pull(wfdDoubleQueue_ *wfdDoubleQueue);
void calculate_freq(wfdQueue_ *wfdQueue);
void wfdNode_destroy(wfdNode_ *wfdNode);
void wfdQueue_destroy(wfdQueue_ *wfdQueue);
void wfdDoubleQueue_destroy(struct wfdDoubleQueue_ *wfdDoubleQueue);

struct thread_* thread_init(thpool_* thpool_p, int id, char *type);
struct thpool_* thpool_init(int num_threads, char *type);
void thpool_destroy(thpool_* thpool_p, int num_threads);

int isDIR(struct stat info);
void* findFILE(void *vargs);
void* findWFD(void *vargs);


/* ================================ ARGS ================================ */


struct args_* args_init() {
	args_ *args = malloc(sizeof(args_));
	if(args == NULL) {
		fprintf(stderr, "args_init(): Could not allocate memory for node\n");
		exit(EXIT_FAILURE);
	}
	return args;
}

void args_destroy(args_* args) {
	free(args);
}


/* ========================== NODES & QUEUES ============================ */


struct node_* node_init(char *path) {
     node_* node_p = malloc(sizeof(struct node_));
	if(node_p == NULL) {
		fprintf(stderr, "node_init(): Could not allocate memory for node\n");
		exit(EXIT_FAILURE);
	}
	node_p->path = path;
	node_p->next = NULL;

	return node_p;
}

struct queue_* queue_init() {
	queue_ *queue_p = malloc(sizeof(struct queue_));
	if(queue_p == NULL) {
		fprintf(stderr, "queue_init(): Could not allocate memory for queue\n");
		exit(EXIT_FAILURE);
	}
	if(pthread_mutex_init(&queue_p->lock, NULL)) {
		fprintf(stderr, "queue_init(): Could not initialize pthread mutex.");
		exit(EXIT_FAILURE);
	}
	queue_p->size = 0;
	queue_p->head = queue_p->rear = NULL;

	return queue_p;
}

void queue_push(queue_ *queue_p, char *path) {
	pthread_mutex_lock(&queue_p->lock);
	if(queue_p->head != NULL) {
		queue_p->rear = queue_p->rear->next = node_init(path);
	} else {
		queue_p->rear = queue_p->head = node_init(path);
	}
	++queue_p->size;
	pthread_mutex_unlock(&queue_p->lock);
}

char* queue_pull(queue_ *queue_p) {
	pthread_mutex_lock(&queue_p->lock);
	if(queue_p->head == NULL) {
		return NULL;
	}
	char *path = queue_p->head->path;
	node_ *temp = queue_p->head->next;
	free(queue_p->head);
	queue_p->head = temp;
	if(temp == NULL) {
		queue_p->rear = NULL;
	}
	--queue_p->size;
	pthread_mutex_unlock(&queue_p->lock);

	return path;
}

void queue_destroy(queue_* queue_p) {
	if(pthread_mutex_destroy(&queue_p->lock)) {
		fprintf(stderr, "queue_destroy(): Could not destroy pthread mutex\n");
	}
	node_ *node = queue_p->head;
	while(node != NULL) {
		node_*temp = node->next;
		free(node);
		node = temp->next;
	}
	free(queue_p);
}


/* ================================ WFD ================================= */


wfdNode_* wfdNode_init(char *word) {
	wfdNode_ *wfdNode = malloc(sizeof(wfdNode_));
	if(wfdNode == NULL) {
		perror("wfdNode_init(): Couldn't allocate memory for wfdNode\n");
		exit(EXIT_FAILURE);
	}
	wfdNode->occurrences = 1;
	wfdNode->freq = 0;
	wfdNode->word = word;
	wfdNode->next = NULL;

	return wfdNode;
}

wfdQueue_* wfdQueue_init(char *file) {
	wfdQueue_ *wfdQueue = malloc(sizeof(wfdQueue_));
	if(wfdQueue == NULL) {
		perror("wfdQueue_init(): Couldn't allocate memory for wfdQueue\n");
		exit(EXIT_FAILURE);
	}
	wfdQueue->file = file;
	wfdQueue->total_occurrences = 0;
	wfdQueue->head = NULL;
	wfdQueue->next = NULL;

	return wfdQueue;
}

wfdDoubleQueue_* wfdDoubleQueue_init() {
	wfdDoubleQueue_ *wfdDoubleQueue = malloc(sizeof(wfdDoubleQueue_));
	if(wfdDoubleQueue == NULL) {
		perror("wfdDoubleQueue_init(): Couldn't allocate memory for wfdDoubleQueue\n");
		exit(EXIT_FAILURE);
	}
	if(pthread_mutex_init(&wfdDoubleQueue->lock, NULL)) {
		fprintf(stderr, "wfdDoubleQueue_init(): Could not initialize pthread lock\n");
		exit(EXIT_FAILURE);
	}
	wfdDoubleQueue->size = 0;
	wfdDoubleQueue->head = NULL;

	return wfdDoubleQueue;
}

wfdNode_* wfdQueue_push(wfdQueue_* wfdQueue, char *word) {
	wfdNode_ *wfdNode;

	if(wfdQueue->head == NULL) {
		wfdNode = wfdQueue->head = wfdNode_init(word);
	} else {
		int ret;
		wfdNode = wfdQueue->head;
		wfdNode_ *prev = NULL;

		while(wfdNode != NULL && (ret = strcmp(word, wfdNode->word)) > 0) {
			prev = wfdNode;
			wfdNode = wfdNode->next;
		}
		if(ret) {
			if(prev == NULL) {
				wfdQueue->head = wfdNode_init(word);
				wfdQueue->head->next = wfdNode;
			} else {
				prev = prev->next = wfdNode_init(word);
				prev->next = wfdNode;
				wfdNode = prev;
			}
		} else ++wfdNode->occurrences;
	}
	++wfdQueue->total_occurrences;

	return wfdNode;
}

void wfdDoubleQueue_push(struct wfdDoubleQueue_ *wfdDoubleQueue, struct wfdQueue_ *wfdQueue) {
	pthread_mutex_lock(&wfdDoubleQueue->lock);
	wfdQueue_ *temp = wfdDoubleQueue->head;
	if(temp == NULL) {
		wfdDoubleQueue->head = wfdQueue;
	} else {
		while(temp->next != NULL) {
			temp = temp->next;
		}
		temp->next = wfdQueue;
	}
	++wfdDoubleQueue->size;
	pthread_mutex_unlock(&wfdDoubleQueue->lock);
}

wfdQueue_* wfdDoubleQueue_pull(wfdDoubleQueue_ *wfdDoubleQueue) {
	if(wfdDoubleQueue->head == NULL) {
		return NULL;
	}
	wfdQueue_ *ret_wfdQ = wfdDoubleQueue->head;
	wfdQueue_ *temp = wfdDoubleQueue->head->next;
	wfdDoubleQueue->head = temp;
	--wfdDoubleQueue->size;

	return ret_wfdQ;
}

void calculate_freq(wfdQueue_ *wfdQueue) {
	wfdNode_ *wfdNode = wfdQueue->head;
	while(wfdNode != NULL) {
		wfdNode->freq = (double)wfdNode->occurrences/(double)wfdQueue->total_occurrences;
		wfdNode = wfdNode->next;
	}
}

void wfdNode_destroy(wfdNode_ *wfdNode) {
	wfdNode_ *temp;
	while(wfdNode != NULL) {
		temp = wfdNode->next;
		free(wfdNode->word);
		free(wfdNode);
		wfdNode = temp;
	}
}

void wfdQueue_destroy(wfdQueue_ *wfdQueue) {
	wfdNode_destroy(wfdQueue->head);
	free(wfdQueue);
}

void wfdDoubleQueue_destroy(struct wfdDoubleQueue_ *wfdDoubleQueue) {
	wfdQueue_ *wfdQueue = wfdDoubleQueue->head;
	while(wfdQueue != NULL) {
		wfdQueue_destroy(wfdQueue);
		wfdQueue = wfdQueue->next;
	}
	pthread_mutex_destroy(&wfdDoubleQueue->lock);
}


/* ========================= THREADS & POOLS  =========================== */


/* Initialise thread */
struct thread_* thread_init(thpool_* thpool_p, int id, char *type) {
	thread_* thread_p = malloc(sizeof(struct thread_));
	if(thread_p == NULL) {
		fprintf(stderr, "thread_init(): Could not allocate memory for thread\n");
		exit(EXIT_FAILURE);
	}

	thread_p->name = malloc(sizeof(char)*(strlen(type)+12));
	if(thread_p->name == NULL) {
		fprintf(stderr, "node_init(): Could not allocate memory for node\n");
		exit(EXIT_FAILURE);
	}
	sprintf(thread_p->name, "%s THREAD #%d", type, id);

	thread_p->id = id;
	thread_p->pthread = 0;
	thread_p->thpool = thpool_p;

	return thread_p;
}

/* Initialise thread pool */
struct thpool_* thpool_init(int num_threads, char *type) {
	thpool_ *thpool_p = malloc(sizeof(struct thpool_));
	if(thpool_p == NULL) {
		fprintf(stderr, "thpool_init(): Could not allocate memory for thread pool\n");
		exit(EXIT_FAILURE);
	}
	if(pthread_mutex_init(&thpool_p->lock, NULL)) {
		fprintf(stderr, "thpool_init(): Could not initalize pthread lock\n");
		exit(EXIT_FAILURE);
	}
	if(pthread_cond_init(&thpool_p->threads_all_idle, NULL)) {
		fprintf(stderr, "thpool_init(): Could not initialize pthread condition\n");
		exit(EXIT_FAILURE);
	}

	thpool_p->threads = malloc(sizeof(thread_)*num_threads);
	for(int i = 0; i < num_threads; ++i) {
		thpool_p->threads[i] = thread_init(thpool_p, i, type);
	}
	thpool_p->threads_keepalive = TRUE;
	thpool_p->manager_ready = FALSE;
	thpool_p->num_threads = num_threads;
	thpool_p->num_threads_working = 0;
	thpool_p->num_threads_alive = 0;

	return thpool_p;
}

void thpool_destroy(thpool_* thpool_p, int num_threads) {
	if(pthread_cond_destroy(&thpool_p->threads_all_idle)) {
		fprintf(stderr, "thpool_destroy(): Could not destroy pthread condition\n");
		exit(EXIT_FAILURE);
	}
	for(int i = 0; i < num_threads; ++i) {
		free(thpool_p->threads[num_threads]);
	}
	free(thpool_p->threads);
	free(thpool_p);
}


/* ========================== GENERAL HELPERS =========================== */


/* Input type checker */
int isDIR(struct stat info) {
	if(S_ISREG(info.st_mode)) return 0;
	else if(S_ISDIR(info.st_mode)) return 1;
	else return -1;
}


/* =========================== MAIN FUNCTIONS =========================== */


/* -------------------------- Manager ------------------------- */
void manageThreads(void *vargs) {
	prctl(PR_SET_NAME, "MANAGER");
	args_ *args = vargs;
	thpool_ *dPool = args->dPool;
	thpool_ *fPool = args->fPool;
	args_destroy(args);

	struct timespec max_wait = {0, 0};

	/* Directory thread */
	pthread_mutex_lock(&dPool->lock);
	if(clock_gettime(CLOCK_REALTIME, &max_wait)) {
		perror("manageThreads(): clock_gettime");
		exit(EXIT_FAILURE);
	}
	max_wait.tv_sec += MAX_WAIT_TIME_IN_SECONDS;
	max_wait.tv_nsec += MAX_WAIT_TIME_IN_NANOSECONDS;
	dPool->manager_ready = TRUE;
	pthread_cond_timedwait(&dPool->threads_all_idle, &dPool->lock, &max_wait);
	//pthread_cond_wait(&dPool->threads_all_idle, &dPool->lock);
	dPool->threads_keepalive = FALSE;
	pthread_mutex_unlock(&dPool->lock);

	/* File thread */
	pthread_mutex_lock(&fPool->lock);
	if(clock_gettime(CLOCK_REALTIME, &max_wait)) {
		perror("manageThreads(): clock_gettime");
		exit(EXIT_FAILURE);
	}
	max_wait.tv_sec += MAX_WAIT_TIME_IN_SECONDS;
	max_wait.tv_nsec += MAX_WAIT_TIME_IN_NANOSECONDS;
	fPool->manager_ready = TRUE;
	pthread_cond_timedwait(&fPool->threads_all_idle, &fPool->lock, &max_wait);
	//pthread_cond_wait(&fPool->threads_all_idle, &fPool->lock);
	fPool->threads_keepalive = FALSE;
	pthread_mutex_unlock(&fPool->lock);
}

/* --------- Enqueue files in directories to the queue -------- */
void* findFILE(void *vargs) {
	args_ *args = vargs;

	queue_* dirq = args->dirq;
	queue_* fileq = args->fileq;
	thread_* thread = args->thread;
	thpool_ *thpool = thread->thpool;
	boolean_ *FILE_ERROR = malloc(sizeof(boolean_));
	*FILE_ERROR = FALSE;

	int ret;
	char *ext = args->ext, *path, *name, *fpath, *needle;     // As in a needle in a haystack.
	struct dirent *entry;
	struct stat info;
	DIR *dir;

	prctl(PR_SET_NAME, thread->name);

	pthread_mutex_lock(&thpool->lock);
	++thpool->num_threads_alive;
	pthread_mutex_unlock(&thpool->lock);

	while(thpool->manager_ready == FALSE) {
	}

	while(thpool->threads_keepalive == TRUE) {
		if(thpool->threads_keepalive == TRUE) {
			while(!dirq->size) {
				if(!thpool->num_threads_working) break;
			}

			pthread_mutex_lock(&thpool->lock);
			++thpool->num_threads_working;
			pthread_mutex_unlock(&thpool->lock);

			while(dirq->size) {
				path = queue_pull(dirq);
				if(path == NULL) continue;
				dir = opendir(path);
				errno = 0;
				if(dir == NULL && errno) {
					perror("findFILE(): Couldn't open file\n");
					*FILE_ERROR = TRUE;
				}

				while((entry = readdir(dir)) != NULL) {
					if(entry == NULL && errno) {
						perror("findFILE(): Couldn't read file\n");
						*FILE_ERROR = TRUE;
					}
					if(!strcmp((name = entry->d_name), ".") || !strcmp(name, "..")) {
						continue;
					}

					fpath = malloc(sizeof(char)*(strlen(path)+strlen(name)+2));
					if(fpath == NULL) {
						fprintf(stderr, "findFILE(): Could not allocate memory for node\n");
						exit(EXIT_FAILURE);
					}
					sprintf(fpath, "%s/%s", path, name);

					errno = 0;
					if(stat(fpath, &info) == -1 && errno) {
						fprintf(stderr, "findFILE(): stat");
						*FILE_ERROR = TRUE;
						continue;
					}
					ret = isDIR(info);
					if(!ret) {
						if(ext[0] == '\0') {
							needle = ".";
							if(strstr(name, needle) == NULL) {
								queue_push(fileq, fpath);
							}
						}
						else {
							needle = ext;
							if(strstr(name, needle) != NULL) {
								queue_push(fileq, fpath);
							}
						}
					} else if(ret == 1) {
						queue_push(dirq, fpath);
					}
				}
			}

			pthread_mutex_lock(&thpool->lock);
			--thpool->num_threads_working;
			if(!thpool->num_threads_working) {
				pthread_cond_broadcast(&thpool->threads_all_idle);
			}
			pthread_mutex_unlock(&thpool->lock);
		}
	}
	pthread_mutex_lock(&thpool->lock);
	--thpool->num_threads_alive;
	pthread_mutex_unlock(&thpool->lock);

	args_destroy(args);

	return (void *) FILE_ERROR;
}

/* ------------- Find word frequency distribution ------------- */
void* findWFD(void *vargs) {
	args_ *args = vargs;

	queue_* fileq = args->fileq;
	thread_* thread = args->thread;
	thpool_ *thpool = thread->thpool;

	boolean_ *FILE_ERROR = malloc(sizeof(boolean_));
	*FILE_ERROR = FALSE;

	wfdDoubleQueue_ *wfdDoubleQueue = args->wfdDoubleQueue;
	thpool_ *dPool = args->dPool;
	wfdQueue_ *wfdQueue;
	int i, offset, ret, fd;
	char buf[BUFSIZE] = "";
	char parsed[BUFSIZE] = "";
	char *word = "";
	char *hold;
	char *file;

	prctl(PR_SET_NAME, thread->name);

	pthread_mutex_lock(&thpool->lock);
	++thpool->num_threads_alive;
	pthread_mutex_unlock(&thpool->lock);

	while(thpool->manager_ready == FALSE) {
	}

	while(thpool->threads_keepalive == TRUE || dPool->threads_keepalive == TRUE) {
		if(thpool->threads_keepalive == TRUE) {
			while(!fileq->size) {
				if(!thpool->num_threads_working) break;
			}

			pthread_mutex_lock(&thpool->lock);
			++thpool->num_threads_working;
			pthread_mutex_unlock(&thpool->lock);

			while(fileq->size) {
				file = queue_pull(fileq);
				if(file == NULL) continue;

				fd = open(file, O_RDONLY);
				wfdQueue = wfdQueue_init(file);
				wfdDoubleQueue_push(wfdDoubleQueue, wfdQueue);

				while((ret = (int)read(fd, buf, BUFSIZE))) {
					if(ret == -1 && errno) {
						perror("findWFD(): Couldn't read file");
						continue;
					}

					offset = 0;
					for(i = 0; i < BUFSIZE; ++i) {
						if(('a' <= buf[i] && 'z' >= buf[i]) || '-' == buf[i]) {
							parsed[i-offset] = buf[i];
							parsed[(i-offset)+1] = '\0';
						} else if('A' <= buf[i] && 'Z' >= buf[i]) {
							parsed[i-offset] = (char)(buf[i]+32);
							parsed[(i-offset)+1] = '\0';
						} else if(' ' == buf[i] || '\n' == buf[i] || ret < BUFSIZE) {
							if(parsed[0] == '\0') continue;

							hold = word;
							word = malloc(sizeof(char)*(strlen(parsed)+strlen(hold)+1));
							if(word == NULL) {
								fprintf(stderr, "findWFD(): Could not allocate memory for word\n");
								exit(EXIT_FAILURE);
							}
							sprintf(word, "%s%s", hold, parsed);
							wfdQueue_push(wfdQueue, word);

							offset = i+1;
							parsed[0] = '\0';
							word = "";
						} else ++offset;
					}

					if(parsed[0] == '\0') continue;

					hold = word;
					word = malloc(sizeof(char)*(strlen(parsed)+strlen(hold)+1));
					if(word == NULL) {
						fprintf(stderr, "findWFD(): Could not allocate memory for word\n");
						exit(EXIT_FAILURE);
					}
					sprintf(word, "%s%s", hold, parsed);
					parsed[0] = '\0';
				}

				if(word[0] != '\0') {
					wfdQueue_push(wfdQueue, word);
				}
				calculate_freq(wfdQueue);
			}

			pthread_mutex_lock(&thpool->lock);
			--thpool->num_threads_working;
			if(!thpool->num_threads_working && dPool->threads_keepalive == FALSE && fileq->size <= 0) {
				pthread_cond_broadcast(&thpool->threads_all_idle);
			}
			pthread_mutex_unlock(&thpool->lock);
		}
	}
	pthread_mutex_lock(&thpool->lock);
	--thpool->num_threads_alive;
	pthread_mutex_unlock(&thpool->lock);

	return (void *) FILE_ERROR;
}

/* Find the Jensen-Shannon distance */
wfdNode_* mean_head(wfdQueue_ *wfdQueue1, wfdQueue_ *wfdQueue2) {
	wfdNode_ *wfdNode1 = wfdQueue1->head;
	wfdNode_ *wfdNode2 = wfdQueue2->head;
	wfdQueue_ *mean = wfdQueue_init("<mean>");
	wfdNode_ *wfdNode;
	int ret;
	double freq;

	char *word, *temp;
	while(wfdNode1 != NULL || wfdNode2 != NULL) {
		if(wfdNode2 == NULL) {             // wfdNode1 != NULL
			temp = wfdNode1->word;
			freq = 0.5*wfdNode1->freq;
			wfdNode1 = wfdNode1->next;
		} else if(wfdNode1 == NULL) {      // wfdNode2 != NULL
			temp = wfdNode2->word;
			freq = 0.5*wfdNode2->freq;
			wfdNode1 = wfdNode2->next;
		} else {                           // wfdNode1 != NULL && wfdNode2 != NULL
			ret = strcmp(wfdNode1->word, wfdNode2->word);
			if(ret > 0) {                 /// wfdNode1 > wfdNode2
				temp = wfdNode2->word;
				freq = 0.5*(wfdNode2->freq);
				wfdNode2 = wfdNode2->next;
			} else if(ret < 0) {          /// wfdNode1 < wfdNode2
				temp = wfdNode1->word;
				freq = 0.5*(wfdNode1->freq);
				wfdNode1 = wfdNode1->next;
			} else {                      /// wfdNode1 == wfdNode2
				temp = wfdNode1->word;
				freq = 0.5*(wfdNode1->freq+wfdNode2->freq);
				wfdNode1 = wfdNode1->next;
				wfdNode2 = wfdNode2->next;
			}
		}

		word = malloc(sizeof(char)*(strlen(temp)+1));
		if(word == NULL) {
			perror("mean_head(): Couldn't allocate memory for word\n");
		}
		strcpy(word, temp);

		wfdQueue_push(mean, word)->freq = freq;
	}
	wfdNode = mean->head;

	return wfdNode;
}
double findKLD(wfdNode_ *mean, wfdNode_ *wfdNode) {
	double KLD = 0;

	while(wfdNode != NULL) {
		// Don't have to check for strcmp > 0 because mean contains every entry in wfdNode.
		if(strcmp(wfdNode->word, mean->word) == 0) {
			KLD += wfdNode->freq*(log2(wfdNode->freq)-log2(mean->freq));
			wfdNode = wfdNode->next;
		}
		mean = mean->next;
	}

	return KLD;
}
void* findJSD(void *vargs) {
	args_ *args = vargs;
	thread_* thread = args->thread;
	thpool_ *thpool = thread->thpool;

	boolean_ *FILE_ERROR = malloc(sizeof(boolean_));
	*FILE_ERROR = FALSE;

	wfdDoubleQueue_ *wfdDoubleQueue = args->wfdDoubleQueue;
	int ID = thread->id;
	int num_threads = args->aPool->num_threads;
	double JSD;

	wfdQueue_ *wfdQueue1, *wfdQueue2;
	wfdNode_ *mean, *temp;

	prctl(PR_SET_NAME, thread->name);

	pthread_mutex_lock(&thpool->lock);
	++thpool->num_threads_alive;
	pthread_mutex_unlock(&thpool->lock);

	pthread_mutex_lock(&thpool->lock);
	++thpool->num_threads_working;
	pthread_mutex_unlock(&thpool->lock);

	wfdQueue1 = wfdDoubleQueue->head;
	for(int i = 0; i < wfdDoubleQueue->size; ++i) {
		if(ID == i%num_threads) {
			wfdQueue2 = wfdQueue1->next;
			while(wfdQueue2 != NULL) {
				mean = mean_head(wfdQueue1, wfdQueue2);
				JSD = sqrt(0.5*findKLD(mean, wfdQueue1->head)+0.5* findKLD(mean, wfdQueue2->head));
				fprintf(stdout, "%0.4lf %s %s\n", JSD, wfdQueue1->file, wfdQueue2->file);

				while(mean != NULL) {
					temp = mean->next;
					free(mean);
					mean = temp;
				}
				wfdQueue2 = wfdQueue2->next;
			}
		}
		wfdQueue1 = wfdQueue1->next;
	}

	pthread_mutex_lock(&thpool->lock);
	--thpool->num_threads_working;
	pthread_mutex_unlock(&thpool->lock);

	pthread_mutex_lock(&thpool->lock);
	--thpool->num_threads_alive;
	pthread_mutex_unlock(&thpool->lock);

	args_destroy(args);

	return (void *) FILE_ERROR;
}


/* =============================== DRIVER =============================== */


int main(int argc, char *argv[]) {
	if(argc == 1) {
		usage();
	} else if(argc < 2) {
		fprintf(stderr, "main(): Incorrect syntax\n");
		exit(EXIT_FAILURE);
	}

	int i, ret;
	boolean_ FILE_ERROR = FALSE, *func_ret;
	int d = 1;
	int f = 1;
	int a = 1;
	args_* args;
	char *ext = "txt";
	queue_* fileq = queue_init();
	queue_* dirq  = queue_init();


	/* Check user-inputs */

	// Add files and directories to queues.
	struct stat info;
	for(i = 1; i < argc; ++i) {
		if(argv[i][0] == '-') break;
		errno = 0;
		if(stat(argv[i], &info) == -1 && errno) {
			fprintf(stderr, "main(): stat");
			FILE_ERROR = TRUE;
		} else {
			if((ret = isDIR(info)) == -1) {
				fprintf(stderr, "main(): Incorrect syntax\n");
				exit(EXIT_FAILURE);
			} else if (ret) {
				queue_push(dirq, argv[i]);
			} else {
				queue_push(fileq, argv[i]);
			}
		}
	}

	// Parse optional parameters.
	for(; i < argc; ++i) {
		if(argv[i][0] != '-') {
			fprintf(stderr, "main:() Incorrect syntax\n");
		}
		if(argv[i][1] == 's') {
			if(argv[i][2] == '\0') ext = "";
			else if(argv[i][2] == '.') ext = argv[i]+3;
			else ext = argv[i]+2;
		} else {
			errno = 0;
			int num = (int)strtol(argv[i]+2, NULL, 10);
			if(num == 0 && errno) {
				fprintf(stderr, "Incorrect syntax\n");
				exit(EXIT_FAILURE);
			}
			switch(argv[i][1]) {
				case 'd': d = num; break;
				case 'f': f = num; break;
				case 'a': a = num; break;
				default : {
					fprintf(stderr,  "Incorrect syntax.\n");
					exit(EXIT_FAILURE);
				}
			}
		}
	}


	/* Calculate JSD */


	thpool_ *dPool  = thpool_init(d, "DIR");
	thpool_ *fPool  = thpool_init(f, "FILE");
	thpool_ *aPool = thpool_init(a, "ANLYS");
	wfdDoubleQueue_ *wfdDoubleQueue = wfdDoubleQueue_init();
	thread_ **threads;

	// Search directories and subdirectories for files of the given extension and add to the file queue.
	threads = dPool->threads;
	for(i = 0; i < d; ++i) {
		args = args_init();
		args->ext = ext;
		args->fileq = fileq;
		args->dirq = dirq;
		args->thread = threads[i];
		threads[i]->id = i;

		if(pthread_create(&threads[i]->pthread, NULL, (void *)findFILE, (void *)args)) {
			fprintf(stderr, "main(): Failed to create directory thread\n");
			exit(EXIT_FAILURE);
		}
	}

	// Calculate the word frequencies distribution (WFD) of all files.
	threads = fPool->threads;
	for(i = 0; i < f; ++i) {
		args = args_init();
		args->fileq = fileq;
		args->dirq = dirq;
		args->dPool = dPool;
		args->thread = threads[i];
		args->wfdDoubleQueue = wfdDoubleQueue;
		if(pthread_create(&threads[i]->pthread, NULL, (void *)findWFD, (void *)args)) {
			fprintf(stderr, "main(): Failed to create pthread\n");
			exit(EXIT_FAILURE);
		}
	}
	args_destroy(args);

	/* Start managing threads */
	args = args_init();
	args->dPool = dPool;
	args->fPool = fPool;
	args->aPool = aPool;
	manageThreads((void *)args);

	// Wait for directory and file threads to finish before continuing to the next section.
	threads = dPool->threads;
	for(i = 0; i < d; ++i) {

		if(pthread_join(threads[i]->pthread, (void **)&func_ret)) {
			fprintf(stderr, "main(): Failed to join  file thread\n");
			exit(EXIT_FAILURE);
		}
		if(*func_ret != FALSE) {
			FILE_ERROR = TRUE;
		}
		free(func_ret);
	}

	threads = fPool->threads;
	for(i = 0; i < f; ++i) {
		if(pthread_join(threads[i]->pthread, (void **)&func_ret)) {
			fprintf(stderr, "main(): Failed to join pthread\n");
			exit(EXIT_FAILURE);
		}
		if(*func_ret != FALSE) {
			FILE_ERROR = TRUE;
		}
		free(func_ret);
	}

	// Calculate the Jenson-Shannon distance (JSD) of all file pairs.

	threads = aPool->threads;
	for(i = 0; i < a; ++i) {
		args = args_init();
		args->thread = threads[i];
		args->aPool = aPool;
		args->wfdDoubleQueue = wfdDoubleQueue;
		threads[i]->id = i;
		if(pthread_create(&threads[i]->pthread, NULL, (void *)findJSD, (void *)args)) {
			fprintf(stderr, "main(): Failed to create pthread\n");
			exit(EXIT_FAILURE);
		}
	}

	// Wait for JSD threads to finish.
	threads = aPool->threads;
	for(i = 0; i < a; ++i) {
		if(pthread_join(threads[i]->pthread, (void **)&func_ret)) {
			fprintf(stderr, "main(): Failed to join pthread\n");
			exit(EXIT_FAILURE);
		}
		if(*func_ret != FALSE) {
			FILE_ERROR = TRUE;
		}
		free(func_ret);
	}


	/* Free allocations and end program */
	queue_destroy(fileq);
	queue_destroy(dirq);
	thpool_destroy(dPool, d);
	thpool_destroy(fPool, f);
	thpool_destroy(aPool, a);
	wfdDoubleQueue_destroy(wfdDoubleQueue);
	if(FILE_ERROR == TRUE) {
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}