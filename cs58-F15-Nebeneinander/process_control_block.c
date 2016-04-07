#include "trap_handlers.h"

PCB *IdlePCB = NULL;
PCB *InitPCB = NULL;

int process_id_counter = 0;

PCB *InitializePCB() {
	TracePrintf(1, "PCB Initializing\n");

	//if there is not enough free frames for the new kernel stack, return NULL;
	if (KERNEL_STACK_FRAMES > FreeFrameCount) {
		TracePrintf(1, "There is not enough free frames for kernel stack!\n");
		return NULL;
	}

	PCB *pcb = (PCB *) malloc(sizeof(PCB));
	if (NULL == pcb) {
		TracePrintf(1, "PCB initialization malloc failed!\n");
		return NULL;
	}

	//initialize all 0
	memset(pcb, 0, sizeof(PCB));

	pcb->pid = ++process_id_counter;

	u_int i;
	for (i = 0; i < KERNEL_STACK_FRAMES; i++) {
		pcb->ksframes[i] = getFreeFrame(Page(KERNEL_STACK_BASE) + i);
	}

	TracePrintf(1, "PCB with pid %d has been initialized\n", pcb->pid);

	return pcb;
}

void DecrementDelayQueue() {
	if (FAILURE == QueueIsEmpty(DelayQueue)) {
		//find the first one which is not going to be removed
		while (0 == --containerOf(DelayQueue->head, PCB, queue_node)->timer) {
			TracePrintf(2, 	"Process %d from the delay queue back"
					" to the ready queue!\n",
					containerOf(DelayQueue->head, PCB, queue_node)->pid);
			Enqueue(ReadyQueue, Dequeue(DelayQueue));
			if (SUCCESS == QueueIsEmpty(DelayQueue)) {
				return;
			}
		}

		Queue_Node *previousQueue_Node	= DelayQueue->head;
		Queue_Node *thisQueue_Node 	= DelayQueue->head->next;

		while(NULL != thisQueue_Node) {
		//regular remove operation with the previous stayed node not NULL
			if (0 == --containerOf(thisQueue_Node, PCB, queue_node)->timer) {
				TracePrintf(2, 	"Process %d from the delay queue back"
						" to the ready queue!\n",
						containerOf(thisQueue_Node, PCB, queue_node)->pid);
				Enqueue(ReadyQueue, thisQueue_Node);
				previousQueue_Node->next = thisQueue_Node->next;
				thisQueue_Node->next = NULL;
				thisQueue_Node = previousQueue_Node->next;
			} else {
				previousQueue_Node = thisQueue_Node;
				thisQueue_Node = thisQueue_Node->next;
			}			
		}

		//update the new tail pointer
		//since this removal is not a standard operation
		//in queue data structure like Enqueue and Dequeue
		DelayQueue->tail = previousQueue_Node;
	}
}

int FreePCB(PCB *doomedProcess) {

	TracePrintf(1, "Freeing process (pid = %d)\n", doomedProcess->pid);
	//remove from PCB tree structure
	if (NULL != doomedProcess->parent) {
		PCB *left_sibling = doomedProcess->parent->child;
		if (left_sibling ==  doomedProcess) {
			doomedProcess->parent->child = doomedProcess->sibling;
		} else {
			while (left_sibling->sibling != doomedProcess) {
				left_sibling = left_sibling->sibling;	
			}
			left_sibling->sibling = doomedProcess->sibling;
		}
	}
	if (NULL != doomedProcess->child) {
		PCB *currentSibling = doomedProcess->child;
		while (NULL != currentSibling) {
			currentSibling->parent = NULL;
			currentSibling = currentSibling->sibling;
		}
	}

	//free the child exit status queue
	FreeQueue(&doomedProcess->childExitStatus);
	free(doomedProcess);
	TracePrintf(1, "Process killed\n");
	return SUCCESS;
}

int InsertPCB(PCB *child) {
	if (NULL == child || NULL == child->parent) {
		TracePrintf(2, "Parent or child is Null!\n");
		return FAILURE;
	}
	if (NULL == child->parent->child) {
		child->parent->child = child;
		return SUCCESS;
	}
	PCB *currChild = child->parent->child;
	while (NULL != currChild->sibling) {
		currChild = currChild->sibling;
	}
	currChild->sibling = child;
	return SUCCESS;
}

