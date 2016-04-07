#include "data_structures.h"

extern KernelContext *KCKSInit(KernelContext *, void *, void *);
extern KernelContext *KCSwitch(KernelContext *, void *, void *);
extern KernelContext *KCKSFree(KernelContext *, void *, void *);
extern int KCSwitchToQueue(Queue *);
extern int KCSwitchToNode(Queue_Node *);
