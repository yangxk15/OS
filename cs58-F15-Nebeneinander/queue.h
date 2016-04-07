
typedef struct queue_node {
	struct queue_node *next;
} Queue_Node;

typedef struct queue {
	Queue_Node *head;
	Queue_Node *tail;
} Queue;

void Enqueue(Queue *, Queue_Node *);
Queue_Node *Dequeue(Queue *);
int QueueIsEmpty(Queue *);
void FreeQueue(Queue *);

extern Queue *ReadyQueue;
extern Queue *DelayQueue;
extern Queue *TransmitQueue[];
extern Queue *ReceiveQueue[];
extern Queue *BufferQueue[];
