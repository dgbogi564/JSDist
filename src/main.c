#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <sys/stat.h>

#include "jsdist/jsdist.h"
#include "thpool/thpool.h"


//////////////////////////////////////////////////////////////////////
// usage:       jsdist {File ...|Directory...} [-dN] [-fN] [-aN] [-sS]
//                  N: positive integers
//                  S: file name suffix
//
//              Recursively obtains all files of the extension "txt"(or
//              from the specified extension) from the directories and
//              files given and calculates each pair's Jensen-Shannon
//              distance.
//
//
//                  -dN:	specifies the # of directory threads used
//                              (default # of directory threads: 1)
//
//                  -fN:	specifies the # of file threads used
//                              (default # of file threads: 1)
//
//                  -aN:	specifies the # of analysis threads used
//                              (default # of analysis threads: 1)
//
//                  -sS:	specifies the file name suffix
//                              (default file suffix: txt)
//////////////////////////////////////////////////////////////////////



char *ext;

thpool_ *dpool;
thpool_ *fpool;
thpool_ *apool;

int main(int argc, char *argv[]) {

    if (argc < 2) {
        usage();
        perror("main(): Incorrect number of arguments");
        exit(EXIT_FAILURE);
    }


	int8_t ERROR = 0;


    ext = "txt";

    uint16_t dnum_threads = 1;
    uint16_t fnum_threads = 1;
    uint16_t anum_threads = 1;


    int offset;

    /* Parse optional parameters */
    for (offset = argc-1; offset >= 0; --offset) {
        if (argv[offset][0] != '-') {
            break;
        }

        char option = argv[offset][1];

        if (option == 's') {

            char chara = argv[offset][2];

            if (chara == '\0') {
                ext = "";
                continue;
            }

            if (chara == '.') {
                ext = argv[offset]+3;
                continue;
            }

            ext = argv[offset]+2;
            continue;
        }


        errno = 0;

        uint32_t num_threads = (uint32_t)strtol(argv[offset]+2, NULL, 10);
        if(num_threads == 0 && errno) {
            usage();
            fprintf(stderr, "main(): %s is not a number\n", argv[offset]+2);
            exit(EXIT_FAILURE);
        }


        switch(option) {
            case 'd':
                dnum_threads = num_threads;
                break;
            case 'f':
                fnum_threads = num_threads;
                break;
            case 'a':
                anum_threads = num_threads;
                break;
            default: {
                usage();
                fprintf(stderr, "main(): %c is not a recognized option\n", option);
                exit(EXIT_FAILURE);
            }
        }
    }


    /* Initialize pools */
    dpool = thpool_init(dnum_threads, "directory");
    if(dpool == NULL) {
        perror("main(): Failed to initialize directory pool");
        exit(EXIT_FAILURE);
    }

    fpool = thpool_init(fnum_threads, "file");
    if(fpool == NULL) {
        perror("main(): Failed to initialize file pool");
        thpool_wait_and_destroy(dpool);
        exit(EXIT_FAILURE);
    }

    apool = thpool_init(anum_threads, "analysis");
    if(fpool == NULL) {
        perror("main(): Failed to initialize analysis pool");
        thpool_wait_and_destroy(dpool);
        thpool_wait_and_destroy(fpool);
        exit(EXIT_FAILURE);
    }


    /* Parse files and directories */
    while(offset) {

        errno = 0;


        struct stat info;

        if (stat(argv[offset], &info) == -1 && errno) {
            perror("main(): stat");
            thpool_wait_and_destroy(dpool);
            thpool_wait_and_destroy(fpool);
            jsdlist_destroy();
            wfdlist_destroy();
            exit(EXIT_FAILURE);
        }

        if (S_ISDIR(info.st_mode)) {
            thpool_add_work(dpool, (void *)get_files, argv[offset]);
        } else if (S_ISREG(info.st_mode)) {
            thpool_add_work(fpool, (void *)get_wfd, argv[offset]);
        } else {
            usage();
            fprintf(stderr, "%s is not a valid file or directory\n", argv[offset]);
            thpool_wait_and_destroy(dpool);
            thpool_wait_and_destroy(fpool);
	        jsdlist_destroy();
	        wfdlist_destroy();
            exit(EXIT_FAILURE);
        }


        offset--;
    }


    /* Wait for file and directory pools to finish and destroy them */
    if(thpool_wait_and_destroy(dpool)) {
        ERROR = -1;
    }

    if(thpool_wait_and_destroy(fpool)) {
        ERROR = -1;
    }


    /* Call analysis threads to work */
    for (uintptr_t i = 0; i < anum_threads; ++i) {
        thpool_add_work(apool, (void *)get_jsd, (void *)i);
    }

    /* Wait for analysis threads to finish and destroy them */
    if(thpool_wait_and_destroy(apool)) {
        ERROR = -1;
    }


    /* Print JSD List */
    jsd_ *jsd = jsdlist.front;
    while(jsd != NULL) {
        printf("%lf %s %s\n", jsd->value, jsd->file1_path, jsd->file2_path);
        jsd = jsd->next;
    }


    /* Destroy lists */
    jsdlist_destroy();
    wfdlist_destroy();
    fplist_destroy();


    if (ERROR) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}