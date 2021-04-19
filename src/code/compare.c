#include "compare.h"

#include <stdio.h>

#include <sys/stat.h>

#include "args.h"
#include "errorh.h"


#include <dirent.h>
void* searchDIR(void *arg) {
	if(DEBUG) setbuf(stdout, NULL);

	args_t *args = arg;
	queue_t *fileq = args->fileq, *dirq = args->dirq;

	int *FILE_ERROR = hmalloc(sizeof(int)), ret;
	*FILE_ERROR = 0;

	char *ext = args->ext, *path, *name, *fpath, *needle;     // As in a needle in a haystack.
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
						if(DEBUG) printf("%-10s %s\n","enqueue:", fpath);
					}
				} else if(name != NULL && name[strlen(ext+1)] == '\0') {
					hpthread_mutex_lock(&fileq->lock);
					enqueue(fileq, fpath);
					hpthread_mutex_unlock(&fileq->lock);
				} else if(ret == 1) enqueue(dirq, fpath);                   // Is Directory.

				fpath = NULL;
			}
		}

		hpthread_mutex_lock(&dirq->lock);
		--dirq->num_working;
		hpthread_mutex_unlock(&dirq->lock);

		while(dirq->num_working > 0) {
			hclock_gettime(CLOCK_REALTIME, &max_wait);
			max_wait.tv_sec += MAX_WAIT_TIME_IN_SECONDS;
			max_wait.tv_nsec += MAX_WAIT_TIME_IN_NANOSECONDS;

			hpthread_mutex_lock(&dirq->lock);

			hpthread_cond_broadcast(&dirq->write_ready);
			hpthread_cond_timedwait(&dirq->write_ready, &dirq->lock, &max_wait);
			hpthread_cond_broadcast(&dirq->write_ready);

			hpthread_mutex_unlock(&dirq->lock);

			if(dirq->size > 0) ++dirq->num_working;
		}
	} while(dirq->num_working > 0);

	return (void *)FILE_ERROR;
}

#include <fcntl.h>
void* findWFD(void *arg) {
	if(DEBUG) setbuf(stdout, NULL);

	args_t *args = arg;

	queue_t *dirq = args->dirq, *fileq = args->fileq;
	wfdLL_t *wfd_repo = args->wfd_repo;
	wfd_t *wfd;
	int i, fd, offset = 0;
	int ret, *retval = hmalloc(sizeof(int));
	char buf[BUFSIZE] = "", parsed[BUFSIZE] = "", *word = "", *hold, *path;

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
			hpthread_mutex_lock(&wfd_repo->lock);
			insert_wfd(wfd_repo, wfd);
			hpthread_mutex_unlock(&wfd_repo->lock);

			if(DEBUG) {
				char *separator = "=================================================================";
				printf(   "\nFILE: %s\n"
		                    "%-18s %-18s %-18s\n%.*s\n", path, "word", "count", "freq", 50, separator);
			}

			while((ret = hread(fd, buf, BUFSIZE))) {
				if(ret == -1) {
					*retval = 1;
					continue;
				}

				for(i = 0; i < BUFSIZE; ++i) {
					if(('a' <= buf[i] && 'z' >= buf[i]) || '-' == buf[i]) {
						parsed[i-offset] = buf[i];
						parsed[(i-offset)+1] = '\0';
					} else if('A' <= buf[i] && 'Z' >= buf[i]) {
						parsed[i-offset] = (char)(buf[i]+32);
						parsed[(i-offset)+1] = '\0';
					} else if(' ' == buf[i] || '\n' == buf[i]) {
						if(parsed[0] == '\0') continue;

						hold = word;
						word = hmalloc(sizeof(char)*(strlen(parsed)+strlen(hold)+1));
						sprintf(word, "%s%s", hold, parsed);
						add_wfdNode(wfd, word);

						offset = i+1;
						parsed[0] = '\0', word = "";
					} else ++offset;
				}
				offset = 0;
				if(parsed[0] == '\0') continue;

				hold = word;
				word = hmalloc(sizeof(char)*(strlen(parsed)+strlen(hold)+1));
				sprintf(word, "%s%s", hold, parsed);
				parsed[0] = '\0';
			}
			if(word[0] != '\0') add_wfdNode(wfd, word);
			word = "";
			calculate_freq(wfd);

			if(DEBUG) puts("");
		}
	} while (dirq->num_working || fileq->size > 0);

	hpthread_mutex_lock(&fileq->lock);
	--fileq->num_working;
	hpthread_mutex_unlock(&fileq->lock);

	return (void *)retval;
}

#include "wfd.h"

wfdNode_t* mean_wfd(wfdNode_t *wfdn1, wfdNode_t *wfdn2) {
	wfd_t *mean = wfd_init("mean");
	wfdNode_t *wfdNode;

	while(wfdn1 != NULL || wfdn2 != NULL) {
		if(wfdn1 == NULL) {                // wnfd1 is null.
			  if(wfdn2 != NULL) {         // wfdn2 is not null.
			  	add_wfdNode(mean, wfdn2->word)->freq = 0.5*wfdn2->freq;
			  	wfdn2 = wfdn2->next;
			  }
		} else if(wfdn2 == NULL) {         // wfdn2 is null, but wfdn1 is not null.
			add_wfdNode(mean, wfdn1->word)->freq = 0.5*wfdn1->freq;
			wfdn1 = wfdn1->next;
		}
		else {                             // Both are not null.
			int ret = strcmp(wfdn1->word, wfdn2->word);
			if        (ret > 0) add_wfdNode(mean, wfdn1->word)->freq = 0.5*wfdn1->freq;
			else if   (ret < 0) add_wfdNode(mean, wfdn1->word)->freq = 0.5*wfdn2->freq;
			else                add_wfdNode(mean, wfdn1->word)->freq = 0.5*(wfdn1->freq+wfdn2->freq);

			wfdn1 = wfdn1->next;
			wfdn2 = wfdn2->next;
		}
	}

	wfdNode = mean->head;
	mean->head = NULL;
	free_wfd(mean);

	return wfdNode;
}

#include <math.h>
double findKLD(wfdNode_t *mean, wfdNode_t *wfdNode) {
	double KLD = 0;

	while(wfdNode != NULL) {
		// Don't have to check for strcmp > 0 because mean contains every entry in wfdNode.
		if(strcmp(wfdNode->word, mean->word) < 0) wfdNode = wfdNode->next;
		else {
			KLD += 0.5*log2(wfdNode->freq/mean->freq);
			wfdNode = wfdNode->next;
			mean = mean->next;
		}
	}

	return KLD;
}

void* findJSD(void *arg) {
	args_t *args = arg;
	hpthread_mutex_lock(&args->lock);
	int ID = args->thread_id;
	hpthread_mutex_unlock(&args->lock);

	double JSD;

	wfdLL_t *wfd_repo = args->wfd_repo;
	wfd_t *wfd1 = wfd_repo->head, *wfd2;

	setbuf(stdout, NULL);
	int i, j;
	for(i = 0; i < wfd_repo->size && wfd1 != NULL; ++i) {
		if(ID == i%args->a) {
			wfd2 = wfd1->next;
			for(j = i; j < wfd_repo->size && wfd2 != NULL; ++i) {
				wfdNode_t *mean = mean_wfd(wfd1->head, wfd2->head);

				JSD = sqrt(0.5*findKLD(mean, wfd1->head)+0.5* findKLD(mean, wfd2->head));
				printf("%lf %s %s", JSD, wfd1->file, wfd2->file);

				wfd2 = wfd2->next;
			}
		}
		wfd1 = wfd1->next;
	}
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
	int a = 1;
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
				case 'a': a = num; break;
				default : {
					fprintf(stderr, "Incorrect syntax.\n");
					exit(EXIT_FAILURE);
				}
			}
		}
	}

	args->d = d, args->f = f, args->a = a;
	pthread_t dTID[d], fTID[f], aTID[a];
	args->wfd_repo = wfdLL_init();

	// Recursively search directories for files of given extension type and add to queue.
	for(i = 0; i < d; ++i) hpthread_create(&dTID[i], NULL, (void *) searchDIR, (void *)args);

	// Calculate the word frequencies distribution (WFD) of all files.
	for(i = 0; i < f; ++i) hpthread_create(&fTID[i], NULL, (void *)findWFD, (void *)args);

	// Wait for directory and file threads to finish before continuing to the next section.
	for(i = 0; i < d; ++i) hpthread_join(dTID[i], (void **)&ptr);
	if(d && ptr != NULL) {
		if(*ptr) FILE_ERROR = 1;
		free(ptr);
	}

	for(i = 0; i < f; ++i) hpthread_join(fTID[i], (void **)&ptr);
	if(f && ptr != NULL) {
		if(*ptr) FILE_ERROR = 1;
		free(ptr);
	}

	// Calculate the Jenson-Shannon distance (JSD) of all file pairs.
	for(i = 0; i < a; ++i) {
		args->thread_id = i;
		hpthread_create(&aTID[i], NULL, (void *)findJSD, (void *)args);
	}

	// Wait for the Jenson-Shannon distance JSD threads to finish.
	for(i = 0; i < a; ++i) hpthread_join(aTID[i], NULL);

	// Free Allocations and End Program.
	free_queue(dirq), free_queue(fileq), free_wfdLL(args->wfd_repo), free(args);
	if(FILE_ERROR) exit(EXIT_FAILURE);
	return EXIT_SUCCESS;
}

