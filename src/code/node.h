#ifndef P2_NODE_H
#define P2_NODE_H

typedef struct node {
	char* data;
	struct node *next;
} node_t;

node_t* node_init(void *data);

#endif //P2_NODE_H
