#ifndef P2_LINKEDLIST_H
#define P2_LINKEDLIST_H

#include <stdlib.h>

#include "node.h"
typedef struct linkedList {
	int size;
	node_t *head;
} linkedList_t;

linkedList_t* linkedList_init();

void add_node(linkedList_t linkedList, void *data);

void free_linkedList(linkedList_t* linkedList);

#endif //P2_LINKEDLIST_H
