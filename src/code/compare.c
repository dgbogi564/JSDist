#include "compare.h"
#include "lock.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <pthread.h>



args_t* args_init() {
    args_t *args = malloc(sizeof(args_t));
    if(args == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    args->ext = NULL;
    args->num_threads = 0;
    args->thread_id = 0;
    args->dirq = NULL;
    args->fileq = NULL;
    args->wfd_repo = NULL;
    args->jsd_repo = init_jsdl();

    return args;
}
int isTYPE(struct stat info) { // need to check for errors
	if(S_ISREG(info.st_mode)) return 1;	// Is file.
	if(S_ISDIR(info.st_mode)) return 2;	// Is directory.
	return 0;									// Is neither.
}

#include <dirent.h>
#include <string.h>
void* searchDIR(void *arg) {
    args_t *args = (args_t *)arg;
    int *retval = malloc(sizeof(int));
    if(retval == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    *retval = 0;

    queue_t *dirq = args->dirq;
    queue_t *fileq = args->fileq;
    char *ext = args->ext;
    int ext_len = (int)strlen(ext);
    int ret;
    char *name;
    char *path;
    char *fpath;
    char *needle;

    struct dirent *entry;
    struct stat info;
    DIR* dir;

    while(dirq->threads_working > 0) {
        while(dirq->size > 0) {
            lock(&dirq->lock);
            path = dequeue(dirq);
            unlock(&dirq->lock);
            if(path == NULL) continue;

            dir = opendir(path);
            if(dir == NULL) {
                perror("opendir");
                *retval = 1;
                continue;
            }

            // Determine if path is a directory or file.
            while((entry = readdir(dir)) != NULL) {
                if( !strcmp(entry->d_name, ".") ||
                    !strcmp(entry->d_name, "..")) continue;

                name = entry->d_name;

                fpath = malloc(sizeof(char)*(strlen(path)+strlen(entry->d_name)+2));
                if(fpath == NULL) {
                    perror("malloc");
                    exit(EXIT_FAILURE);
                }
                fpath[0] = '\0';

                strcat(fpath, path);
                strcat(fpath, "/");
                strcat(fpath, entry->d_name);

                if(stat(fpath, &info) == -1) {
                    perror("stat");
                    *retval = 1;
                    continue;
                }

                ret = isTYPE(info);
                if(ret == 1) {                              // Is a file.
                    needle = malloc(sizeof(char)*(ext_len+2));
                    needle[0] = '\0';
                    strcat(needle, ".");
                    strcat(needle, ext);


                    if(ext[0] == '\0') {
                        if(strstr(name, needle) == NULL) {
                            lock(&fileq->lock);
                            enqueue(fileq, fpath);
                            unlock(&fileq->lock);
                        }
                    } else {
                        if((    name = strstr(name, needle)) != NULL &&
                           name[strlen(ext)+1] == '\0')
                        {

                            lock(&fileq->lock);
                            enqueue(fileq, fpath);
                            unlock(&fileq->lock);
                        }
                    }

                }
                else if(ret == 2){                          // Is a directory.
                    lock(&dirq->lock);
                    enqueue(dirq, fpath);
                    unlock(&dirq->lock);
                }
            }
        }

        lock(&dirq->lock);
        dirq->threads_working--;
        while(dirq->threads_working > 0) {
            pthread_cond_broadcast(&dirq->write_ready);
            pthread_cond_wait(&dirq->write_ready, &dirq->lock);
            pthread_cond_broadcast(&dirq->write_ready);
        }
        if(dirq->threads_working > 0) dirq->threads_working++;
        unlock(&dirq->lock);
    }

    return retval;
}

#include <unistd.h>
#include <fcntl.h>
void* findWFD(void *arg) {
    args_t *args = (args_t *)arg;

    queue_t *dirq = args->dirq;
	queue_t *fileq = args->fileq;
	wfdl_t *wfd_repo = args->wfd_repo;
	wfd_t *wfd;
    int ret;
    int *retval = malloc(sizeof(int));
    *retval = 0;
    int i;
    int j;
    int fd;
    int offset;
    char buf[BUFSIZE] = "";
    char temp[BUFSIZE] = "";
    char *word = NULL;
    char *hold = NULL;

    while(dirq->threads_working > 0) {
        while(fileq->size > 0) {
            lock(&fileq->lock);
            char *path = dequeue(fileq);
            unlock(&fileq->lock);
            if(path == NULL) continue;



            wfd = wfd_init(path);
            lock(&wfd_repo->lock);
            insert_wfd(wfd_repo, wfd);
            unlock(&wfd_repo->lock);
            fd = open(path, O_RDONLY);

            offset = 0;
            j = 0;

            while((ret = (int)read(fd, buf, BUFSIZE))) {
                if(ret < 0) {
                    perror("read");
                    *retval = 1;
                }

                for(i = 0; i < BUFSIZE; ++i) {
                    if(('a' <= buf[i] &&
                        'z' >= buf[i]) ||
                        '-' == buf[i])
                    {
                        temp[j-offset] = buf[i];
                        temp[(j-offset)+1] = '\0';
                    } else if(  'A' <= buf[i] &&
                                'Z' >= buf[i])
                    {
                        temp[j-offset] = (char)(buf[i]+32);
                        temp[(j-offset)+1] = '\0';
                    } else if(  ' ' == buf[i] ||
                                '\n' == buf[i])
                    {
                        if(word != NULL) {
                            hold = word;
                            word = malloc(sizeof(char)*(strlen(hold)+strlen(temp)+1));
                            if(word == NULL) {
                                perror("malloc");
                                exit(EXIT_FAILURE);
                            }
                            word[0] = '\0';
                            strcat(word, hold);
                            strcat(word, temp);
                            free(hold);
                            hold = NULL;
                        } else {
                            word = malloc(sizeof(char)*(strlen(temp)+1));
                            if(word == NULL) {
                                perror("malloc");
                                exit(EXIT_FAILURE);
                            }
                            word[0] = '\0';
                            strcat(word, temp);
                        }

                        insert_wfdn(wfd, word);
                        temp[0] = '\0';
                        word = NULL;
                        j = -1; // Will be 0 on next loop.
                        offset = 0;
                    } else offset++;

                    j++;
                }
                if(temp[0] == '\0') continue;
                if(word != NULL) {
                    hold = word;
                    word = malloc(sizeof(char)*(strlen(hold)+strlen(temp)+1));
                    if(word == NULL) {
                        perror("malloc");
                        exit(EXIT_FAILURE);
                    }
                    word[0] = '\0';
                    strcat(word, hold);
                    strcat(word, temp);
                    free(hold);
                    hold = NULL;
                } else {
                    word = malloc(sizeof(char)*(strlen(temp)+1));
                    if(word == NULL) {
                        perror("malloc");
                        exit(EXIT_FAILURE);
                    }
                    word[0] = '\0';
                    strcat(word, temp);
                }

                temp[0] = '\0';
            }

            calculate_freq(wfd);
        }
    }

    return retval;
}

#include <math.h>
wfdn_t* mean_wfd(wfdn_t *node1, wfdn_t *node2) {
    int ret;
	wfdn_t *mean;

    if(node1 != NULL && node2 != NULL) ret = strcmp(node1->word, node2->word);
    if(node2 == NULL || ret > 0) {
        mean = wfdn_init(node1->word);
        mean->freq = 0.5*(node1->freq);

        node1 = node1->next;
    } else if(node1 == NULL || ret < 0) {
        mean = wfdn_init(node2->word);
        mean->freq = 0.5*(node2->freq);

        node2 = node2->next;
    } else {
        mean = wfdn_init(node1->word);
        mean->freq = 0.5*(node1->freq + node2->freq);

        node1 = node1->next;
        node2 = node2->next;
    }

    wfdn_t *temp = mean;
    while(node1 != NULL || node2 != NULL) {
        if(node1 != NULL && node2 != NULL) {
            ret = strcmp(node1->word, node2->word);
            if(ret > 0) {
                temp->next = wfdn_init(node1->word);
                temp = temp->next;
                temp->freq = 0.5*(node1->freq);

                node1 = node1->next;
            } else if(ret < 0) {
                temp->next = wfdn_init(node2->word);
                temp = temp->next;
                temp->freq = 0.5*(node2->freq);

                node2 = node2->next;
            } else {
                temp->next = wfdn_init(node1->word);
                temp = temp->next;
                temp->freq = 0.5*(node1->freq + node2->freq);

                node1 = node1->next;
                node2 = node2->next;
            }
        }

        if(node1 != NULL && node2 == NULL) {
            temp->next = wfdn_init(node1->word);
            temp = temp->next;
            temp->freq = 0.5*(node1->freq);

            node1 = node1->next;
        } else if(node2 != NULL && node1 == NULL) {
            temp->next = wfdn_init(node2->word);
            temp = temp->next;
            temp->freq = 0.5*(node2->freq);

            node2 = node2->next;
        }
    }

	return mean;
}
double findKLD(wfdn_t *mean, wfdn_t *node) {
	double KLD = 0;
	int ret;

	while(node != NULL) {
        ret = strcmp(mean->word, node->word);
        // The case (ret < 0) has been omitted because the mean list contains every word in head1 and head2.
        if(ret > 0) node = node->next;
        else {
            KLD += node->freq*log2(node->freq/mean->freq);
            mean = mean->next;
            node = node->next;
	    }
	}

	return KLD;
}
void findJSD(void *arg) {
    args_t *args = (args_t *)arg;

	wfdl_t *wfd_repo = args->wfd_repo;
	jsdl_t *jsd_repo = args->jsd_repo;
	int thr_id = args->thread_id;
	int thr_num = args->num_threads;
	int size = wfd_repo->size;
    if(size < 2) {
        fprintf(stderr, "At least 2 non-empty files are required to calculate the Jensen-Shannon distance (JSD).");
        exit(EXIT_FAILURE);
    }
	wfdn_t *mean;
	int i;
	int j;
	char *string;

    wfd_t *wfdNode1 = wfd_repo->head;
    wfd_t *wfdNode2 = wfd_repo->head->next;

	for(i = 0; i < size && wfdNode1 != NULL; ++i) {
	    if(i%thr_num == thr_id) {
	        for(j = 0; j < size && wfdNode2 != NULL; ++j) {
	            if(j > i) {
                    // Calculate the mean word frequency distribution (WFD).
                    mean = mean_wfd(wfdNode1->head, wfdNode2->head);
                    // Calculate the mean Kullback-Leibler divergence (KLD) of both files
                    // and square root the result to get the Jensen-Shannon distance (JSD).
                    double JSD = sqrt(0.5*findKLD(mean, wfdNode1->head)+0.5*findKLD(mean, wfdNode2->head));
                    string = malloc(sizeof(char)*(strlen(wfdNode1->file)+strlen(wfdNode2->file)+BUFSIZE));
                    sprintf(string, "%lf %s %s\n", JSD, wfdNode1->file, wfdNode2->file);

                    lock(&jsd_repo->lock);
                    insert_jsdn(jsd_repo, string);
                    unlock(&jsd_repo->lock);

                    wfdNode2 = wfdNode2->next;

                    free(mean);
	            }
	        }
	    }

	    wfdNode1 = wfdNode1->next;
	}

	return;
}

void usage() {
	printf(	"usage: compare {File ...|Directory...} [%c[1m-d%c[0mN] [%c[1m-f%c[0mN] [%c[1m-a%c[0mN] [%c[1m-s%c[0mS]\n"
		 		"\tN: positive integers\n"
	  			"\tS: file name suffix\n"
	  			"Recursively obtains all files of the extension \"txt\" (or from the specified extension)\n"
   				"from the directories and files given and calculates each pair's Jensen-Shannon distance.\n\n"
	   			"-dN:\tspecifies the # of directory threads used\n"
	   			"\t\t(default # of directory threads: 1)\n\n"
			   	"-fN:\tspecifies the # of file threads used\n"
			   	"\t\t(default # of file threads: 1)\n\n"
			   	"-aN:\tspecifies the # of analysis threads used\n"
			   	"\t\t(default # of analysis threads: 1)\n\n"
	   			"-sS:\tspecifies the file name suffix\n"
	   			"\t\t(default file suffix: txt)\n",
			   	27, 27, 27, 27, 27, 27, 27, 27);

	exit(EXIT_FAILURE);
}
#include <errno.h>
int main(int argc, char * argv[]) {
	if(argc == 1) usage();
	if(argc < 2) {
		fprintf(stderr, "Incorrect syntax.");
		exit(EXIT_FAILURE);
	}

	int i, ret, *ptr_ret;
	int FILE_ERROR = 0;
	args_t *args = args_init();
	queue_t *dirq = args->dirq = queue_init();
	queue_t *fileq = args->fileq = queue_init();
	// Default file extension & # of directory, file and analysis threads.
	int d = 1;
	int f = 1;
	int a = 1;
	args->ext = "txt";

    // Add files and directories to their respective queues.
	struct stat info;
	for(i = 1; (i < argc) && (argv[i][0] != '-'); ++i) {
		if(stat(argv[i], &info) == -1) {
			perror("stat");
			FILE_ERROR = 1;
			continue;
		}

		ret = isTYPE(info);
		if(ret == 1) enqueue(fileq, argv[i]);      // Is a file.
		else if(ret == 2) enqueue(dirq, argv[i]);  // Is a directory.
	}
	// Parse optional user-arguments.
	for(; i < argc; ++i) {
		if(argv[i][0] != '-') {
			fprintf(stderr, "Incorrect syntax.\n");
            free(args);
			free_queue(dirq);
            free_queue(fileq);
			exit(EXIT_FAILURE);
		}

		if(argv[i][1] == 's') {
			 if(argv[i][2] == '\0') args->ext = "";
			 else {
			     if(argv[i][2] == '.') args->ext = argv[i]+3;
			     else args->ext = argv[i]+2;
			 }
		} else {
			errno = 0;
			int num = (int)strtol(argv[i]+2, NULL, 10);
			if(!errno && num >= 0) {
				switch(argv[i][1]) {
					case 'd': d = num;
						break;
					case 'f': f = num;
						break;
					case 'a': a = num;
						break;
					default: {
						fprintf(stderr, "Incorrect syntax.\n");
						exit(EXIT_FAILURE);
					}
				}
			} else {
				fprintf(stderr, "%s is not a positive integer.", argv[i]+2);
				exit(EXIT_FAILURE);
			}
		}
	}

	dirq->num_threads = dirq->threads_working = d;
	fileq->num_threads = fileq->threads_working = f;

	pthread_t dTID[d];
    pthread_t fTID[f];
    pthread_t aTID[a];

	args->wfd_repo = wfdl_init();

	// Recursively search directories for files of given extension type and add to queue.
	for(i = 0; i < d; ++i) {
        args->thread_id = i;
	    if(pthread_create(&dTID[i], NULL, (void *)searchDIR, (void *)args)) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
	    }
	}
	// Calculate the word frequencies distribution (WFD) of all files.
	for(i = 0; i < f; ++i) {
	    if(pthread_create(&fTID[i], NULL, (void *)findWFD, (void *)args)) {
	        perror("pthread_create");
            exit(EXIT_FAILURE);
	    }
	}

	// Wait for directory and file threads to finish before continuing to the next section.
	for(i = 0; i < d; ++i) {
		if(pthread_join(dTID[i], (void **)&ptr_ret)) {
		    perror("pthread_join");
		    exit(EXIT_FAILURE);
		}
		if(*ptr_ret == 1 || *ptr_ret == 3) FILE_ERROR = 1;
		free(ptr_ret);
	}

	for(i = 0; i < f; ++i) {
        if(pthread_join(fTID[i], (void **)&ptr_ret)) {
		    perror("pthread_join");
		    exit(EXIT_FAILURE);
		}
        if(*ptr_ret == 1 || *ptr_ret == 3) FILE_ERROR = 1;
        free(ptr_ret);
    }

	// Calculate the Jenson-Shannon distance (JSD) of all file pairs.
	for(i = 0; i < a; ++i) {
		args->thread_id = i;
		args->num_threads = a;
		if(pthread_create(&aTID[i], NULL, (void *)findJSD, args)) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
	}

	for(i = 0; i < a; ++i) {
        if(pthread_join(aTID[i], NULL)) {
		    perror("pthread_join");
		    exit(EXIT_FAILURE);
		}
    }

	// Free Allocations and End Program.
	free_queue(dirq);
	free_queue(fileq);
    free_wfdl(args->wfd_repo);
    print_and_free_jsdl(args->jsd_repo);
	free(args);

	if(FILE_ERROR) return(EXIT_FAILURE);
	return EXIT_SUCCESS;
}
