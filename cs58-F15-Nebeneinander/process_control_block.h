/*******   PCB DATA STRUCTURE
 *
 *	As each new process is created, it is assigned a pid, incremented by
 *	one from the last pid issued.
 *	
 *	It also inherits a user/kernelcontext from either the hardware or it's
 *	parent process.
 *	
 *	The process's page table is embedded in the PCB, as it is a necessary
 *	part of any process, it makes sense to have it contiguous and easily 
 *	malloc'd/freed with the PCB.
 *
 *	Each pcb has it's own kernel stack frames, which it keeps track of in
 *	ksframes. This is the only record of these frames when the pcb is not
 *	the currently active process.
 *
 *	Each pcb has the following pointers to relate to other pcbs
 *		1) parent pointer
 *		2) child pointer
 *		3) sibling pointer
 *	These pointers constitute a "Left-child right-sibling binary tree"
 *	where each pcb has at most one child directly linked to it
 *	and other children are connected by sibling links to the 
 *	initial child.
 *
 *	For using PCBs in linked-lists & queues, we elected to
 *	use the 'Linux' model, i.e: a queue node embedded in the
 *	pcb struct.
 *
 *	Each PCB also has a number of book keeping fields:
 *		stackBase, brk, userDataEnd delimit the segments in userspace.
 *		timer is an int that is currently used for Delay
 *			but it could be used for any other function
 *			as long as a process would not be in Delay and
 *			that function concurrently.
 *		waitingOnChild is a simple flag with
 *				0 = not waiting
 *				1 = waiting
 *			for a single child to exit and hand over a status
 *		childExitStatus is a queue, for the opposite case from
 *			waitingOnChild. When a child exits and
 *			the parent is not already waiting, it enqueues
 *			it's status & pid in this queue.
 */

#ifndef _process_control_block_h
#define _process_control_block_h

#include "page_table.h"
#include "queue.h"

#define KERNEL_STACK_FRAMES KERNEL_STACK_MAXSIZE / PAGESIZE

#define CurrentPCB() ((PCB *) KERNEL_STACK_BASE)

#define NOT_WAITING  0
#define WAITING      1

typedef struct Exit_Status {
	int pid;
	int status;
	Queue_Node queue_node;
} ExitStatus;

typedef struct Process_Control_Block { 

	//Process identification data
	int pid;
	UserContext uctxt;
	KernelContext kctxt;
	PTE pagetable[MAX_PT_LEN];

	//keep track of the frames mapped to kernel stack
	int ksframes[KERNEL_STACK_FRAMES];
	
	//tree data structure
	struct Process_Control_Block *parent;
	struct Process_Control_Block *sibling;
	struct Process_Control_Block *child;

	//linked list queue_node
	Queue_Node queue_node;

	/*** Personal bookkeeping */
	//stack break, and data pointers
	void *stackBase;
	void *brk;
	void *userDataEnd;
	//timer for delay
	int timer;
	//flag for wait
	int waitingOnChild;
	//Queue of child exit states
	Queue childExitStatus;
} PCB;

extern PCB *InitPCB;
extern PCB *IdlePCB;

extern int process_id_counter;

extern PCB *InitializePCB();
extern void DecrementDelayQueue();
extern int FreePCB(PCB *);
extern int InsertPCB(PCB *);

#endif
