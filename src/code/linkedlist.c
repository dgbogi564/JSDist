#include "linkedlist.h"
#include "errorh.h"

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;


linkedList_t* linkedList_init() {
	linkedList_t *linkedList = hmalloc(sizeof(linkedList_t));
	//int size = 0;
	linkedList->head = NULL;
	//node_t* head = NULL;
	return linkedList;
}

void addNode(linkedList_t *linkedList, void *data) {
	hpthread_mutex_lock(&lock);

	node_t* node = linkedList->head;
	if(node == NULL) linkedList->head = node_init(data);
	else {
		while(node->next != NULL) node = node->next;
		node->next = node_init(data);
	}
	++linkedList->size;

	hpthread_mutex_unlock(&lock);
}

void free_linkedList(linkedList_t *linkedList) {
	hpthread_mutex_lock(&lock);

	node_t *node = linkedList->head, *temp;
	while(node != NULL) {
		temp = node->next;
		free(node);
		node = temp;
	}

	hpthread_mutex_unlock(&lock);
	free(linkedList);
}