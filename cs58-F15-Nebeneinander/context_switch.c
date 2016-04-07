#include "context_switch.h"

/** Kernel Context Kernel Stack Initialization: 
 *	PCBToInit: 	PCB whose Kernel stack needs to be initialized
 *	nextPCB:	Placeholder
 *	note: does not switch to nextPCB
 */
KernelContext *KCKSInit(KernelContext * kc, void *PCBToInit, void *nextPCB) {

	char *indent = "KCKSInit:";
	TracePrintf(1, "%s Start!\n", indent);

	//initialize the kernel context
	((PCB *) PCBToInit)->kctxt = *kc;

	//record the current interface pfn
	int interfacePFN = KernelPageTable[INTERFACE].pfn;

 	PCB *CurrentPCB = KS->CurrentPCB;
 	KS->CurrentPCB = (PCB *) PCBToInit;
 
	//populate the contents of the frames with the current frames
	//fill each kernel stack frame of PCBToInit with the contents
	//of the current kernel stack, so it will match the copy of the
	//kernel context
	u_int i;
	for (i = 0; i < KERNEL_STACK_FRAMES; i++) {
		changePTE(INTERFACE, -1, ((PCB *) PCBToInit)->ksframes[i]);
		memcpy(	Address(INTERFACE), 
			Address(Page(KERNEL_STACK_BASE) + i),
			PAGESIZE);
	}

	//restore current pcb pointer
	KS->CurrentPCB = CurrentPCB;

	//restore the original interface pfn
	changePTE(INTERFACE, -1, interfacePFN);

	Enqueue(ReadyQueue, &((PCB *) PCBToInit)->queue_node);

	TracePrintf(1, "%s Exit!\n", indent);
	return kc;
}

/** Kernel Context Switch: 
 *	This is the normal Kernel Context Switch function 
 *	in which the kernel context will magically switch 
 *	from currentPCB to nextPCB
 */
KernelContext *KCSwitch(KernelContext * kc, void *currentPCB, void *nextPCB) {

	char *indent = "KCSwitch:";
	TracePrintf(1, "%s Start!\n", indent);

	if (NULL == nextPCB) {
		TracePrintf(1, "%s nextPCB == NULL!\n", indent);
		return kc;
	}

	TracePrintf(1, "%s Current pid = %d, Next pid = %d!\n", 
			indent, ((PCB *) currentPCB)->pid,
				((PCB *) nextPCB)->pid);

	KS->CurrentPCB->kctxt = *kc;

	// replace old ks with new frames
	u_int i;
	for (i = 0; i < KERNEL_STACK_FRAMES; i++) {
		changePTE(Page(KERNEL_STACK_BASE) + i, -1, 
				((PCB *) nextPCB)->ksframes[i]);
        }

	//replace the user pagetable and flush the TLB
	WriteRegister(REG_PTBR1, (u_int) KS->CurrentPCB->pagetable);
  	WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);

	TracePrintf(1, "%s Exit!\n", indent);

	return &(KS->CurrentPCB->kctxt);

}

/** Kernel Stack Free:
 *	PCBtoFree's kernel stack and PCB will be freed
 *	Should mainly be called by the kernel call Exit
 *	Has more responsibility than it's predecessor, KCKSInit
 *	and actually switches to nextPCB
 */
KernelContext *KCKSFree(KernelContext * kc, void *PCBtoFree, void *nextPCB) {

	char *indent = "KCKSFree";
	TracePrintf(1, "%s Start!\n", indent);

	TracePrintf(2, "%s Free frames of dying process page table\n", indent);
	disablePTE(MAX_PT_LEN, NUM_VPN);

	TracePrintf(2, "%s Disable old kernel stack pages\n", indent);
	u_int i;
	int frame_to_free;
	for (i = 0; i < KERNEL_STACK_FRAMES; i++) {
		frame_to_free = changePTE(Page(KERNEL_STACK_BASE) + i, -1, 
					 ((PCB *) nextPCB)->ksframes[i]);
		setFrameFree(frame_to_free);
	}
	
	FreePCB((PCB *) PCBtoFree);

	//replace the user pagetable and flush the TLB
	WriteRegister(REG_PTBR1, (u_int) KS->CurrentPCB->pagetable);
  	WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);
	
	return &(KS->CurrentPCB->kctxt);

}
/* Wrapper for switching to the result of a dequeue
 * Mostly for switching to ready queue */
int KCSwitchToQueue(Queue *Q) {
	int rc = KernelContextSwitch(KCSwitch,
			(void *) KS->CurrentPCB,
			(void *) containerOf(Dequeue(Q), PCB, queue_node));
	return rc;
}
/* Wrapper for switching to the owner of a specific queue node
 * Mostly for ipc and tty context switching */
int KCSwitchToNode(Queue_Node *QN){
	int rc = KernelContextSwitch(KCSwitch,
			(void *) KS->CurrentPCB,
			(void *) containerOf(QN, PCB, queue_node));
	return rc;
}
