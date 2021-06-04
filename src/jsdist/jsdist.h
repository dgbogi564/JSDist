//////////////////////////////////////////////////////////////////////
//	filename: 	jsdist.h
//
//	purpose:	Calculates the Jensen-Shannon distance of files given.
//
//	important:  Must call fplist_destroy() before the end of the program.
//
//              Must use a dynamically allocated word when calling
//              get_wfd().
//
//  usage:      jsdist {File ...|Directory...} [-dN] [-fN] [-aN] [-sS]
//                  N: positive integers
//                  S: file name suffix
//
//              Recursively obtains all files of the extension "txt"(or
//              from the specified extension) from the directories and
//              files given and calculates each pair's Jensen-Shannon
//              distance.
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
//
/////////////////////////////////////////////////////////////////////



#ifndef JSDIST_H
#define JSDIST_H

#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>

#include "../thpool/thpool.h"



/* ========================== STRUCTURES ============================ */


/* Jensen-Shannon distance */
typedef struct jsd_ {
	char *file1_path;           /* Path to first file           */
	char *file2_path;           /* Path to second file          */
	double value;               /* JSD value                    */
	struct jsd_ *next;
} jsd_;

/* Jensen-Shannon distance list */
typedef struct jsdlist_ {
    volatile uint32_t size;     /* List size                    */
    jsd_ *front;                /* Pointer to first JSD         */
    jsd_ *rear;                 /* Pointer to last JSD          */
    pthread_mutex_t lock;       /* Thread lock                  */
} jsdlist_;





/* ======================== GLOBAL VARIABLES ======================== */


extern jsdlist_ jsdlist;

extern thpool_ *dpool;
extern thpool_ *fpool;
extern thpool_ *apool;

extern char *ext;





/* ========================== PROTOTYPES ============================ */


void jsdlist_destroy();
void wfdlist_destroy();
void fplist_destroy();

int8_t get_files(char *dir_path);
int8_t get_wfd(char *file_path);
int8_t get_jsd(uint16_t ID);

void usage();





#endif //JSDIST_H
