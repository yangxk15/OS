#include "data_structures.h"

Queue *ReadyQueue	= NULL;
Queue *DelayQueue	= NULL;
Queue *BufferQueue[NUM_TERMINALS] = {0};
Queue *ReceiveQueue[NUM_TERMINALS] = {0};
Queue *TransmitQueue[NUM_TERMINALS] = {0};


void Enqueue(Queue *Q, Queue_Node *toEnqueue) {
	TracePrintf(3, "Enqueuing node\n");
	if (NULL == toEnqueue) {
		TracePrintf(0, "ERROR Enqueue Null Pointer!");
		Halt();
	}
	if (NULL == Q->head) {
		Q->head = toEnqueue;
		Q->tail = toEnqueue;
	} else {
		Q->tail->next = toEnqueue;
		Q->tail = toEnqueue;
	}
}

Queue_Node *Dequeue(Queue *Q) {
	TracePrintf(3, "Dequeuing node\n");
	if (NULL == Q->head) {
		TracePrintf(3, "Nothing to dequeue\n");
		return NULL;
	}

	Queue_Node *toDequeue = Q->head;

	if (Q->head == Q->tail) {
		Q->head = NULL;
		Q->tail = NULL;
	} else {
		Q->head = Q->head->next;
	}
	toDequeue->next = NULL;
	return toDequeue;

}

int QueueIsEmpty(Queue *Q) {
	if (Q->head == NULL && Q->tail == NULL) {
		TracePrintf(2, "Queue is empty!\n");
		return SUCCESS;
	} 
	TracePrintf(2, "Queue is not empty!\n");
	return FAILURE;
}

void FreeQueue(Queue *Q) {
	Queue_Node *currentQueue_Node = Q->head;
	Queue_Node *nextQueue_Node;
	while (NULL != currentQueue_Node) {
		nextQueue_Node = currentQueue_Node->next;
		free(containerOf(currentQueue_Node, ExitStatus, queue_node));
		currentQueue_Node = nextQueue_Node;
	}
}
