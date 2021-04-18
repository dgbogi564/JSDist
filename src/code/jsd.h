#ifndef P2_JSD_H
#define P2_JSD_H

#include <pthread.h>

typedef struct jsdNode {
    char *string;
    struct jsdNode *next;
} jsdn_t;

typedef struct jsdList {
    jsdn_t *head;
    pthread_mutex_t lock;
} jsdl_t;

jsdl_t* init_jsdl();

jsdn_t* init_jsdn(char *string);

void insert_jsdn(jsdl_t* jsdList, char *string);

void print_and_free_jsdl(jsdl_t *jsdNode);

#endif //P2_JSD_H
