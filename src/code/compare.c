#include <stdio.h>

#include <sys/stat.h>

#include "args.h"
#include "errorh.h"

#ifndef BUFSIZE
#define BUFSIZE 256
#endif

#ifndef DEBUG
#define DEBUG 1
#endif


#include <dirent.h>
#ifndef MAX_WAIT_TIME_IN_SECONDS
#define MAX_WAIT_TIME_IN_SECONDS (5)
#endif
void* searchDIR(void *arg) {
	if(DEBUG) setbuf(stdout, NULL);

	args_t *args = arg;
	queue_t *fileq = args->fileq, *dirq = args->dirq;

	int *FILE_ERROR = hmalloc(sizeof(int)), ret;
	*FILE_ERROR = 0;

	char *ext = args->ext;
	char *path, *name, *fpath, *needle;     // As in a needle in a haystack.
	struct dirent *entry;
	struct stat info;
	DIR *dir;

	struct timespec max_wait = {0, 0};

	do {
		hpthread_mutex_lock(&dirq->lock);
		++dirq->num_working;
		hpthread_mutex_unlock(&dirq->lock);

		while(dirq->size > 0) {
			path = dequeue(dirq);
			if(path == NULL) continue;
			dir = hopendir(path, FILE_ERROR);

			while((entry = hreaddir(dir, FILE_ERROR)) != NULL) {
				if(!strcmp((name = entry->d_name), ".") || !strcmp(name, "..")) continue;

				fpath = hmalloc(sizeof(char)*(strlen(path)+strlen(name)+2));
				sprintf(fpath, "%s/%s", path, name);

				if((*FILE_ERROR = hstat(fpath, &info))) continue;
				if(!(ret = isTYPE(info))) {                                 // Is File.
					needle = hmalloc(sizeof(char)*(strlen(ext)+2));
					if(sprintf(needle, ".%s", ext) < 0) {
						perror("sprintf");
						exit(EXIT_FAILURE);
					} // REMINDER: look into how to create an error function for sprintf.

					if(  (ext[0] == '\0' && (name = strstr(name, needle)) == NULL) ||
						(name != NULL && name[strlen(ext)+1 == '\0'])) {
						enqueue(fileq, fpath);
						if(DEBUG) printf("enqueue: %s\n", fpath);
						hpthread_mutex_lock(&fileq->lock);
						hpthread_cond_broadcast(&fileq->write_ready);
						hpthread_mutex_unlock(&fileq->lock);
					}
				} else if(name != NULL && name[strlen(ext+1)] == '\0') {
					hpthread_mutex_lock(&fileq->lock);
					enqueue(fileq, fpath);
					hpthread_mutex_unlock(&fileq->lock);
				} else if(ret == 1) enqueue(dirq, fpath);                   // Is Directory.

				fpath == NULL;
			}
		}

		--dirq->num_working;
		while(dirq->num_working > 0) {
			hpthread_mutex_lock(&dirq->lock);

			hpthread_cond_broadcast(&dirq->write_ready);

			hclock_gettime(CLOCK_REALTIME, &max_wait);
			max_wait.tv_sec += MAX_WAIT_TIME_IN_SECONDS;
			hpthread_cond_timedwait(&dirq->write_ready, &dirq->lock, &max_wait);

			hpthread_cond_broadcast(&dirq->write_ready);

			hpthread_mutex_unlock(&dirq->lock);
		}
		if(dirq->num_working > 0) ++dirq->num_working;

	} while(dirq->num_working > 0);

	pthread_cond_signal(&fileq->write_ready);
	hpthread_mutex_unlock(&fileq->lock);

	return (void *)FILE_ERROR;
}

#include <fcntl.h>
void* findWFD(void *arg) {
	args_t *args = arg;

	queue_t *dirq = args->dirq, *fileq = args->fileq;
	wfdLL_t *wfd_repo = args->wfd_repo;
	wfd_t *wfd;
	int i, j, fd, offset;
	int ret, *retval = malloc(sizeof(int));
	char buf[BUFSIZE] = "", temp[BUFSIZE] = "", *path;
	char *word = NULL, *hold = NULL;

	hpthread_mutex_lock(&fileq->lock);
	fileq->num_working++;
	hpthread_mutex_unlock(&fileq->lock);
	do {
		while(fileq->size > 0) {
			hpthread_mutex_lock(&fileq->lock);
			path = dequeue(fileq);
			hpthread_mutex_unlock(&fileq->lock);
			if(path == NULL) continue;

			fd = open(path, O_RDONLY);
			wfd = wfd_init(path);
			insert_wfd(wfd_repo, wfd);

			offset = j = 0;
			while((ret = (int)read(fd, buf, BUFSIZE))) {
				if(ret < 0) {
					perror("read");
					*retval = 1;
					break;
				}

				for(i = 0; i < BUFSIZE; ++i) {
					if(  ('a' <= buf[i] &&
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
							word = hmalloc(sizeof(char)*(strlen(hold)+strlen(temp)+1));
							sprintf(word, "%s%s", hold, temp);
							strcat(word, hold);
							strcat(word, temp);
							free(hold);
							hold = NULL;
						} else {
							word = hmalloc(sizeof(char)*(strlen(temp)+1));
							sprintf(word, "%s", temp);
						}

						add_wfdNode(wfd, word);
						temp[0] = '\0';
						word = NULL;
						j = -1;        // Will be 0 on next loop.
						offset = 0;
					} else offset++;
					j++;
				}
				if(temp[0] == '\0') continue;
				if(word != NULL) {
					hold = word;
					word = hmalloc(sizeof(char)*(strlen(hold)+strlen(temp)+1));
					sprintf(word, "%s%s", hold, temp);
					hold = NULL;
				} else {
					word = hmalloc(sizeof(char)*(strlen(temp)+1));
					sprintf(word, "%s", temp);
				}

				temp[0] = '\0';
			}

			calculate_freq(wfd);
		}
	} while (dirq->num_working);


	--fileq->num_working;
	return (void *)retval;
}

#include "wfd.h"
wfdNode_t* mean_wfd(wfdNode_t *node1, wfdNode_t *node2) {
	int ret;
	wfdNode_t *mean;

	if(node1 != NULL && node2 != NULL) ret = strcmp(node1->word, node2->word);
	if(node2 == NULL || ret > 0) {
		mean = wfdNode_init(node1->word);
		mean->freq = 0.5*(node1->freq);

		node1 = node1->next;
	} else if(node1 == NULL || ret < 0) {
		mean = wfdNode_init(node2->word);
		mean->freq = 0.5*(node2->freq);

		node2 = node2->next;
	} else {
		mean = wfdNode_init(node1->word);
		mean->freq = 0.5*(node1->freq + node2->freq);

		node1 = node1->next;
		node2 = node2->next;
	}

	wfdNode_t *temp = mean;
	while(node1 != NULL || node2 != NULL) {
		if(node1 != NULL && node2 != NULL) {
			ret = strcmp(node1->word, node2->word);
			if(ret > 0) {
				temp->next = wfdNode_init(node1->word);
				temp = temp->next;
				temp->freq = 0.5*(node1->freq);

				node1 = node1->next;
			} else if(ret < 0) {
				temp->next = wfdNode_init(node2->word);
				temp = temp->next;
				temp->freq = 0.5*(node2->freq);

				node2 = node2->next;
			} else {
				temp->next = wfdNode_init(node1->word);
				temp = temp->next;
				temp->freq = 0.5*(node1->freq + node2->freq);

				node1 = node1->next;
				node2 = node2->next;
			}
		}

		if(node1 != NULL && node2 == NULL) {
			temp->next = wfdNode_init(node1->word);
			temp = temp->next;
			temp->freq = 0.5*(node1->freq);

			node1 = node1->next;
		} else if(node2 != NULL && node1 == NULL) {
			temp->next = wfdNode_init(node2->word);
			temp = temp->next;
			temp->freq = 0.5*(node2->freq);

			node2 = node2->next;
		}
	}

	return mean;
}
/*
double findKLD(wfdNode_t *mean, wfdNode_t *wfdNode) {

}*/
/*
void* findJSD(void *arg) {
	args_t *args = arg;
	//wfdLL_t *wfd_repo = args->wfd_repo;
	//linkedList_t *jsd_repo = args->jsd_repo;

	//int ID = --args->thread_id;
}*/

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

int main(int argc, char *argv[]) {
	if(argc == 1) usage();
	else if(argc < 2) {
		fprintf(stderr, "Incorrect syntax.");
		exit(EXIT_FAILURE);
	}

	args_t* args = args_init();
	// Default parameters.
	int d = 1;
	int f = 1;
	//TODO int a = 1;
	args->ext = "txt";

	int FILE_ERROR = 0, i, ret, *ptr;

	queue_t *dirq = args->dirq = queue_init(), *fileq = args->fileq = queue_init();

	// Add initial user-input files and directories to their respective queues.
	struct stat info;
	for(i = 1; i < argc && (argv[i][0] != '-'); ++i) {
		FILE_ERROR = hstat(argv[i], &info);

		if(!(ret = isTYPE(info))) enqueue(fileq, argv[i]);
		else if(ret == 1) enqueue(dirq, argv[i]);
	}

	// Parse optional parameters.
	for(; i < argc; ++i) {
		if(argv[i][0] != '-') {
			fprintf(stderr, "Incorrect syntax.\n");
			free(args), free_queue(dirq), free_queue(fileq);
			exit(EXIT_FAILURE);
		}

		if(argv[i][1] == 's') {
			if(argv[i][2] == '\0') args->ext = "";
			else if(argv[i][2] == '.') args->ext = argv[i]+3;
			else args->ext = argv[i]+2;
		} else {
			int num = hstrtol(argv[i]+2, NULL, 10);
			switch(argv[i][1]) {
				case 'd': d = num; break;
				case 'f': f = num; break;
				//case 'a': a = num; break;
				default : {
					fprintf(stderr, "Incorrect syntax.\n");
					exit(EXIT_FAILURE);
				}
			}
		}
	}

	dirq->num_threads = d;
	fileq->num_threads = f;

	pthread_t dTID[d], fTID[f]; //aTID[a];
	args->wfd_repo = wfdLL_init();

	// Recursively search directories for files of given extension type and add to queue.
	for(i = 0; i < d; ++i) hpthread_create(&dTID[i], NULL, (void *) searchDIR, (void *)args);

	// Calculate the word frequencies distribution (WFD) of all files.
	for(i = 0; i < f; ++i) hpthread_create(&fTID[i], NULL, (void *)findWFD, (void *)args);

	// Wait for directory and file threads to finish before continuing to the next section.
	for(i = 0; i < d; ++i) hpthread_join(dTID[i], (void **)&ptr);
	if(*ptr) FILE_ERROR = 1;
	free(ptr);

	for(i = 0; i < f; ++i) hpthread_join(fTID[i], (void **)&ptr);
	if(*ptr) FILE_ERROR = 1;
	free(ptr);
	/*
	// TODO Calculate the Jenson-Shannon distance (JSD) of all file pairs.

	args->thread_id = 0;
	for(i = 0; i < a; ++i) {
		args->thread_id = i;
		hpthread_create(&aTID[i], NULL, (void *)findJSD, (void *)args);
	}

	// Wait for the Jenson-Shannon distance JSD threads to finish.
	for(i = 0; i < a; ++i) hpthread_join(aTID[i], NULL);
	 */

	// Free Allocations and End Program.
	free(args),free_queue(dirq), free_queue(fileq), free_wfdLL(args->wfd_repo);
	if(FILE_ERROR) exit(EXIT_FAILURE);
	return EXIT_SUCCESS;
}

