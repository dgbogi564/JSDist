#include "jsd.h"
#include <stdio.h>
#include <stdlib.h>

jsdl_t* init_jsdl() {
    jsdl_t* jsdList = malloc(sizeof(jsdl_t));
    if(jsdList == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    jsdList->head = NULL;
    if(pthread_mutex_init(&jsdList->lock, NULL)) {
        perror("pthread_mutex_init");
        exit(EXIT_FAILURE);
    }

    return jsdList;
}

jsdn_t* init_jsdn(char *string) {
    jsdn_t *jsdNode = malloc(sizeof(jsdn_t));
    if(jsdNode == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    jsdNode->string = string;
    jsdNode->next = NULL;

    return jsdNode;
}

void insert_jsdn(jsdl_t* jsdList, char *string) {
    if(jsdList->head == NULL) jsdList->head = init_jsdn(string);
    else {
        jsdn_t* temp = jsdList->head;
        while(temp->next != NULL) {
            temp = temp->next;
        }
        temp->next = init_jsdn(string);
    }
}

void print_and_free_jsdl(jsdl_t *jsdList) {
    jsdn_t *jsdNode = jsdList->head;
    jsdn_t *temp;
    while(jsdNode != NULL) {
        puts( jsdNode->string);
        temp = jsdNode->next;
        free(jsdNode->string);
        jsdNode = temp;
    }

    if(pthread_mutex_destroy(&jsdList->lock)) {
        perror("pthread_mutex_destroy");
        exit(EXIT_FAILURE);
    }

    free(jsdNode);
}