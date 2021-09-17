#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <pthread.h>

#include <errno.h>

#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

#include <string.h>

#include <math.h>

#include "../thpool/thpool.h"
#include "jsdist.h"


#ifndef BUFSIZE
#define BUFSIZE 255
#endif

#ifdef  JSDIST_DEBUG
    #define JSDIST_DEBUG 1
#else
    #define JSDIST_DEBUG 0
#endif

#if defined(DISABLE_PRINT) == 0 || JSDIST_DEBUG
    #define err(format, ...) fprintf(stderr, format, ##__VA_ARGS__)
#else
    #define err(str)
#endif





/* ========================== PROTOTYPES ============================ */


/* Word frequency */
typedef struct word_ {
    uint32_t occurrences;           /* Number of occurrences within file            */
    double freq;                    /* Frequency of word within file                */
    char *word;                     /* Word                                         */
    struct word_ *next;             /* Pointer to next word frequency               */
} word_;

/* Word frequency distribution */
typedef struct wfd_ {
    uint32_t size;                  /* Number of unique words within file           */
    uint32_t total_occurrences;     /* Total occurrences within file                */
    char *file_path;                /* File path                                    */
    word_ *front;                   /* Pointer to first word frequency              */
    word_ *rear;                    /* Pointer to last word frequency               */
    struct wfd_ *next;              /* Pointer to next word frequency distribution  */
    pthread_mutex_t lock;           /* Thread lock                                  */
} wfd_;

/* Word frequency distribution list */
typedef struct wfdlist_ {
	volatile uint32_t size;         /* Number of files                              */
	wfd_ *front;                    /* Pointer to first wfd                         */
	wfd_ *rear;                     /* Pointer to last wfd                          */
	pthread_mutex_t lock;           /* Thread lock                                  */
} wfdlist_;



/* File path */
typedef struct fp_ {
	char *file_path;                /* File path                                    */
	struct fp_ *next;               /* Pointer to next file path node               */
} fp_;

/* File path list */
typedef struct fplist_ {
	volatile uint32_t size;         /* Number of files                              */
	fp_ *front;                     /* Pointer to first fp                          */
	fp_ *rear;                      /* Pointer to last fp                           */
	pthread_mutex_t lock;           /* Thread lock                                  */
} fplist_;



/* ======================= GLOBAL VARIABLES ========================= */


jsdlist_ jsdlist = { .size = 0, .front = NULL, .rear = NULL, .lock = PTHREAD_MUTEX_INITIALIZER };
wfdlist_ wfdlist = { .size = 0, .front = NULL, .rear = NULL, .lock = PTHREAD_MUTEX_INITIALIZER };
fplist_ fplist   = { .size = 0, .front = NULL, .rear = NULL, .lock = PTHREAD_MUTEX_INITIALIZER };



/* ========================= FILE PATH LIST ========================= */


fp_* fp_init(char *file_path) {

	fp_ *fp = malloc(sizeof(fp_));
	if(fp == NULL) {
		err("fp_init(): Failed to initialize fp (file path) node\n");
		return NULL;
	}
	fp->file_path = file_path;
	fp->next = NULL;

	return fp;
}

int8_t fplist_push(char *file_path) {

	fp_ *fp = fp_init(file_path);
	if(fp == NULL) {
		return -1;
	}

	pthread_mutex_lock(&fplist.lock);

		if(fplist.front == NULL) {
			fplist.front = fp;
			fplist.rear = fp;
		} else {
			fplist.rear->next = fp;
			fplist.rear = fp;
		}

		fplist.size++;

	pthread_mutex_unlock(&fplist.lock);


	return 0;
}

void fplist_destroy() {
	fp_ *fp = fplist.front;
	while(fp != NULL) {
		fp_ *temp = fp->next;
		free(fp->file_path);
		free(fp);
		fp = temp;
	}
	fplist.rear = NULL;
}


/* ================== WORD-FREQUENCY DISTRIBUTION =================== */


word_* word_init(char *word) {

    word_ *wordfreq = malloc(sizeof(word_));
    if (wordfreq == NULL) {
        err("word_init(): Failed to initialize word frequency\n");
        return NULL;
    }

    wordfreq->word = word;
    wordfreq->occurrences = 1;
    wordfreq->next = NULL;


    return wordfreq;
}



wfd_* wfd_init(char *file_path) {

    wfd_ *wfd = malloc(sizeof(wfd_));
    if (wfd == NULL) {
        err("Failed to initialize wfd (word frequency distribution)\n");
        return NULL;
    }

    if (pthread_mutex_init(&wfd->lock, NULL)) {
        err("Failed to initialize mutex\n");
        free(wfd);
        return NULL;
    }

    wfd->size = 0;
    wfd->total_occurrences = 0;
    wfd->file_path = file_path;
    wfd->front = NULL;
    wfd->rear = NULL;
    wfd->next = NULL;


    return wfd;
}


void wfd_destroy(wfd_ *wfd) {

    word_ *wordfreq = wfd->front;
    while(wordfreq != NULL) {
        word_ *temp = wordfreq->next;
        free(wordfreq->word);
        wordfreq->word = NULL;
        free(wordfreq);
        wordfreq = temp;
    }
    wfd->front = NULL;

    free(wfd->file_path);
    wfd->file_path = NULL;
    free(wfd);
}


int8_t wfd_push(wfd_ *wfd, char *word) {

	if(wfd == NULL) {
		err("wfd_push(): wfd (word frequency distribution) not initialized\n");
	}

    /* If wfd is empty */
    if (wfd->size <= 0) {
        wfd->front = word_init(word);
        if (wfd->front == NULL) {
            return -1;
        }
        wfd->rear = wfd->front;
        wfd->size++;
    } else {

        word_ *infront = wfd->front;
        word_ *prev;
        infront = wfd->front;
        int32_t cmp;


        /* Find the the position of the new word should be in lexicographically */
        while (infront != NULL && (cmp = strcmp(word, infront->word)) > 0) {
            prev = infront;
            infront = infront->next;
        }

        /* Check if word is in wfd */
        if (cmp) {
            word_ *temp;
            if (prev == NULL) {
                temp = wfd->front = word_init(word);
            } else {
                temp = prev->next = word_init(word);
            }

            if (temp == NULL) {
                return -1;
            }


            temp->next = infront;
            wfd->size++;

        } else {
            infront->occurrences++;
        }
    }


    wfd->total_occurrences++;
    return 0;
}

void wfdlist_destroy() {
    wfd_ *wfd = wfdlist.front;
    while(wfd != NULL) {
        wfd_ *temp = wfd->next;
        wfd_destroy(wfd);
        wfd = temp;
    }
    wfdlist.front = NULL;
    wfdlist.rear = NULL;
}


int8_t wfdlist_push(wfd_ *wfd) {

    if (wfd == NULL) {
	    err("wfdlist_push(): wfd (word frequency distribution) is not initialized\n");
        return -1;
    }


    pthread_mutex_lock(&wfdlist.lock);

        if (wfdlist.front == NULL) {
            wfdlist.front = wfd;
            wfdlist.rear = wfd;
        } else {
            wfdlist.rear->next = wfd;
            wfdlist.rear = wfd;
        }

		wfdlist.size++;

    pthread_mutex_unlock(&wfdlist.lock);


    return 0;
}





/* ================== JENSEN-SHANNON DISTANCE LIST ================== */


jsd_* jsd_init(char *file1_path, char *file2_path, double value) {
    jsd_ *jsd = malloc(sizeof(jsd_));
    if (jsd == NULL) {
        err("jsd_init(): Failed to initialize jsd (Jensen-Shannon distance)\n");
        return NULL;
    }

    jsd->file1_path = file1_path;
    jsd->file2_path = file2_path;
    jsd->value = value;
    jsd->next = NULL;

    return jsd;
}

void jsdlist_destroy() {
    jsd_ *jsd = jsdlist.front;
    while (jsd != NULL) {
        jsd_ *temp = jsd->next;
        free(jsd);
        jsd = temp;
    }
    jsdlist.front = NULL;
    jsdlist.rear = NULL;
}


int8_t jsdlist_push(jsd_ *jsd) {

	if(jsd == NULL) {
		err("jsdlist_push(): jsd (Jensen-Shannon distance) is not initialized\n");
		return -1;
	}


    /* Push jsd to list */
    pthread_mutex_lock(&jsdlist.lock);

        if (jsdlist.front == NULL) {
            jsdlist.front = jsd;
            jsdlist.rear = jsd;
        } else {
            jsdlist.rear->next = jsd;
            jsdlist.rear = jsd;
        }

		jsdlist.size++;

    pthread_mutex_unlock(&jsdlist.lock);

    return 0;
}





/* ==================== JENSEN-SHANNON DISTANCE ===================== */


int8_t get_files(char *dir_path) {

    if (dir_path == NULL) {
        err("get_files(): Directory path is no initialized\n");
        return -1;
    }


    errno = 0;

    DIR *dir = opendir(dir_path);
    if (dir == NULL && errno) {
        err("get_files(): Failed to open directory\n");
        return -1;
    }


    int8_t return_value = 0;


    /* Enqueue files and subdirectories */
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {

        if (entry == NULL && errno) {
            err("get_files(): Failed to read file\n");
            return_value = -1;
            continue;
        }

        /* Skip current and containing folder */
        char *name = entry->d_name;
        if (strcmp(name, ".") == 0 ||
           strcmp(name, "..") == 0) {
            continue;
        }


        /* Get full file path */
        uint16_t fp_len = strlen(dir_path) + strlen(name) + 2;
        char *file_path = malloc(fp_len*sizeof(char));
        if (file_path == NULL) {
            err("get_files(): Failed to initialize file path\n");
            return_value = -1;
            continue;
        }
        sprintf(file_path, "%s%s", dir_path, name);

        errno = 0;
        struct stat info;
        if (stat(file_path, &info) == -1 && errno) {
            err("get_files(): stat\n");
            return_value = -1;
            continue;
        }

        /* Check if path points to a file or directory */
        if (S_ISREG(info.st_mode)) {

            /* Enqueue files to file pool if it matches the correct extension */
            char *needle;
            if (ext[0] == '\0') {
                needle = ".";
                if (strstr(name, needle) == NULL) {
                    thpool_add_work(fpool, (void *)get_wfd, (void *)file_path);
                }
            } else {
                needle = ext;
                if (strstr(name, needle) != NULL) {
                    thpool_add_work(fpool, (void *)get_wfd, (void *)file_path);
                }
            }
        } else if (S_ISDIR(info.st_mode)) {

            /* Add subdirectory to directory pool */

            thpool_add_work(dpool, (void *)get_files, (void *)file_path);
        } else {
            free(file_path);
        }
    }


    closedir(dir);
    return return_value;
}



void calculate_frequency(wfd_ *wfd) {

    uint32_t total_occurrences = wfd->total_occurrences;

    word_ *wordfreq = wfd->front;

    while (wordfreq != NULL) {
        wordfreq->freq = (double)wordfreq->occurrences/total_occurrences;
        wordfreq = wordfreq->next;
    }
}


int8_t get_wfd(char *file_path) {

    if (file_path == NULL) {
        return -1;
    }


    errno = 0;
    int32_t fd = open(file_path, O_RDONLY);
    if (fd == -1 && errno) {
        err("get_wfd(): Failed to open file\n");
        return -1;
    }

    wfd_ *wfd = wfd_init(file_path);
    if (wfd == NULL) {
        err("get_wfd(): Failed to initialize wfd\n");
        close(fd);
        wfd_destroy(wfd);
        return -1;
    }

	char *word = malloc(sizeof(char));
	if(word == NULL) {
		err("get_wfd(): Failed to initialize word\n");
		close(fd);
		return -1;
	}
	word[0] = '\0';

	char parsed[BUFSIZE];
    char buf[BUFSIZE];
    ssize_t bytes;


    /* Search file and add words to wfd */
    errno = 0;
    while ((bytes = read(fd, buf, BUFSIZE))) {

        if (bytes == -1 && errno) {
            err("get_wfd(): Failed to read file\n");
            close(fd);
            return -1;
        }


        size_t offset = 0;
        char *temp;

        for (size_t i = 0; i < bytes; ++i) {
            if (('a' <= buf[i] && 'z' >= buf[i]) || '-' == buf[i]) {
            	/* Is a lowercase letter */
                parsed[i-offset] = buf[i];
                parsed[(i-offset)+1] = '\0';
            } else if ('A' <= buf[i] && 'Z' >= buf[i]) {
            	/* Is an uppercase letter */
                parsed[i-offset] = (char)(buf[i] + 32);
                parsed[(i-offset)+1] = '\0';
            } else if (' ' == buf[i] || '\n' == buf[i] || '\0' == buf[i]) {
            	/* Is a whitespace character and possibly at the end of the file */

            	if (parsed[0] == '\0') {
                    continue;
                }


                temp = word;

                word = malloc((strlen(temp) + strlen(parsed) + 1)*sizeof(char));
                if (word == NULL) {
                    err("get_wfd(): Failed to initialize word\n");
                    free(temp);
                    close(fd);
                    return -1;
                }
                sprintf(word, "%s%s", temp, parsed);
                free(temp);
                temp = NULL;

                if ('\0' != buf[i] || bytes < BUFSIZE) {

                	wfd_push(wfd, word);

	                word = malloc(sizeof(char));
	                if(word == NULL) {
		                err("get_wfd(): Failed to initialize word\n");
		                close(fd);
		                return -1;
	                }
	                word[0] = '\0';
                }


                parsed[0] = '\0';
	            offset = i+1;
            } else {
	            /* Is none of the above */
                offset++;
            }
        }


        if(parsed[0] != '\0') {
	        temp = word;

	        word = malloc((strlen(temp) + strlen(parsed) + 1)*sizeof(char));
	        if (word == NULL) {
		        err("get_wfd(): Failed to initialize word\n");
		        free(temp);
		        close(fd);
		        return -1;
	        }
	        sprintf(word, "%s%s", temp, parsed);
	        free(temp);
	        temp = NULL;
        }
    }

	if (word[0] != '\0') {
		wfd_push(wfd, word);
	} else {
		free(word);
	}

	/* Calculate every word's frequency */
    calculate_frequency(wfd);

	/* Push wfd to wfd list */
	wfdlist_push(wfd);


    close(fd);
    return 0;
}


int8_t get_jsd(uint16_t ID) {

    pthread_barrier_wait(&apool->barrier);


    if(wfdlist.size < 2) {
        usage();
        err("get_jsd(): Need at least two(2) files to calculate jsd (Jensen-Shannon distance)\n");
        return -1;
    }


    int8_t return_value = 0;


	wfd_ *wfd1 = wfdlist.front;

    for (uint32_t i = ID; i < wfdlist.size-2; i+=apool->num_threads) {

	    wfd_ *wfd2 = wfd1->next;

        double kld1 = 0;
        double kld2 = 0;

        while (wfd2 != NULL) {

            word_ *wfd1_node = wfd1->front;
            word_ *wfd2_node = wfd2->front;

            /* Get Kullback-Leibler Divergence */
            while (wfd1_node != NULL || wfd2_node != NULL) {
                /* If one of the wfd node's are NULL:
                 *
                 *      mean_freq = wfd_node->freq/2
                 *           (the wfd_node that isn't null or the wfd_node
                 *            whose word comes first lexicographically)
                 *
                 *      wfd_node->freq/mean_freq = 2
                 *      log2(wfd_node->freq/mean_freq) = log(2)
                 */


                /* If wfd1's node is NULL */
                if (wfd1_node == NULL) {
                    kld2 += wfd2_node->freq*(log2(2));
                    wfd2_node = wfd2_node->next;
                    continue;
                }


                /* If wfd2's node is NULL */
                if (wfd2_node == NULL) {
                    kld1 += wfd1_node->freq*(log2(2));
                    wfd1_node = wfd1_node->next;
                    continue;
                }


                /* If both wfd nodes are not NULL */
                int32_t cmp = strcmp(wfd1_node->word, wfd2_node->word);
                if (cmp > 0) {
                    kld2 += wfd2_node->freq*(log2(2));
	                wfd2_node = wfd2_node->next;
                } else if (cmp < 0) {
                    kld1 += wfd1_node->freq*(log2(2));
	                wfd1_node = wfd1_node->next;
                } else {
                    double mean_freq = (wfd1_node->freq + wfd2_node->freq)/2;
                    kld1 += wfd1_node->freq*(log2(wfd1_node->freq/mean_freq));
                    kld2 += wfd2_node->freq*(log2(wfd2_node->freq/mean_freq));
	                wfd1_node = wfd1_node->next;
	                wfd2_node = wfd2_node->next;
                }
            }

            /* Calculate and insert JSD into JSD list */
            jsd_ *jsd = jsd_init(wfd1->file_path, wfd2->file_path, sqrt(kld1/2 + kld2/2));
            if (jsdlist_push(jsd)) {
                return_value = -1;
            }

            wfd2 = wfd2->next;
        }


        wfd1 = wfd1->next;
    }


    return return_value;
}





/* ============================== USAGE ============================= */


void usage() {
    fprintf(stderr, "usage: compare {File ...|Directory...} [%c[1m-d%c[0mN] [%c[1m-f%c[0mN] [%c[1m-a%c[0mN] [%c[1m-s%c[0mS]\n",
            27, 27, 27, 27, 27, 27, 27, 27);
	fprintf(stderr,"\tN: positive integers\n");
	fprintf(stderr,"\tS: file name suffix\n");
	fprintf(stderr,"\n");
	fprintf(stderr,"Recursively obtains all files of the extension \"txt\" (or from the specified extension)\n");
    fprintf(stderr,"from the directories and files given and calculates each pair's Jensen-Shannon distance.\n");
	fprintf(stderr,"\n");
	fprintf(stderr,"\n");
	fprintf(stderr,"-dN:\tspecifies the # of directory threads used\n");
	fprintf(stderr,"\t\t(default # of directory threads: 1)\n");
	fprintf(stderr,"\n");
	fprintf(stderr,"-fN:\tspecifies the # of file threads used\n");
	fprintf(stderr,"\t\t(default # of file threads: 1)\n");
	fprintf(stderr,"\n");
	fprintf(stderr,"-aN:\tspecifies the # of analysis threads used\n");
	fprintf(stderr,"\t\t(default # of analysis threads: 1)\n");
	fprintf(stderr,"\n");
	fprintf(stderr,"-sS:\tspecifies the file name suffix\n");
	fprintf(stderr,"\t\t(default file suffix: txt)\n");
	fprintf(stderr,"\n");
	fprintf(stderr,"\n");
}
