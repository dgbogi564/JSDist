#include "node.h"
#include "errorh.h"

node_t* node_init(void *data) {
	node_t* node = hmalloc(sizeof(node_t));
	node->data = data;
	node->next = NULL;
	return node;
}