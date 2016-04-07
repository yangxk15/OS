#include "trap_handlers.h"
#include "context_switch.h"

u_int KernelDataStart = 0;
u_int KernelDataEnd = 0;
u_int KernelBrk = 0;
u_int FreeFrameCount = 0;

int VM_ENABLED = 0;

KernelStack *KS = NULL;

void DoIdle(void) {
	while(1) {
		TracePrintf(0, "********************************************************************************\n");
		TracePrintf(0, "***************************  DoIlde is running!  *******************************\n");
		TracePrintf(0, "********************************************************************************\n");
		Pause();
	}
}

void SetKernelData(void *_KernelDataStart, void *_KernelDataEnd) {
	
	//store args as globals for KernelStart to use 
	KernelDataStart	= (u_int) _KernelDataStart;
	KernelDataEnd	= (u_int) _KernelDataEnd;

}

void KernelStart(char *cmd_args[], unsigned int pmem_size, UserContext *uctxt) {
	//mark the initial KernelBrk
	KernelBrk = KernelDataEnd;

	//FreeFrameCount is only used page_table.c
	FreeFrameCount = pmem_size / PAGESIZE;

	ipc_table = (ObjectNode **) malloc(sizeof(ObjectNode *) * HASH_LEN);
	if (NULL == ipc_table) {
		TracePrintf(0, "ipc_table cannot be initialized!\n");
		return;
	}
	memset(ipc_table, 0, sizeof(ObjectNode *) * HASH_LEN);

	ReadyQueue	= (Queue *) malloc(sizeof(Queue));
	DelayQueue	= (Queue *) malloc(sizeof(Queue));
	if (NULL == ReadyQueue) {
		TracePrintf(0, "ReadyQueue cannot be initialized!\n");
		return;
	}
	if (NULL == DelayQueue) {
		TracePrintf(0, "DelayQueue cannot be initialized!\n");
		return;
	}

	memset(ReadyQueue,	0, sizeof(Queue));
	memset(DelayQueue,	0, sizeof(Queue));

	u_int i;
	for (i = 0; i < NUM_TERMINALS; i++) {
		BufferQueue[i]		= (Queue *) malloc(sizeof(Queue));
		ReceiveQueue[i]		= (Queue *) malloc(sizeof(Queue));
		TransmitQueue[i]	= (Queue *) malloc(sizeof(Queue));
		if (NULL == BufferQueue[i]) {
			TracePrintf(0, "BufferQueue[%d] cannot be initialized!\n", i);
			return;
		}
		if (NULL == ReceiveQueue[i]) {
			TracePrintf(0, "ReceiveQueue[%d] cannot be initialized!\n", i);
			return;
		}
		if (NULL == TransmitQueue[i]) {
			TracePrintf(0, "TransmitQueue[%d] cannot be initialized!\n", i);
			return;
		}


		memset(BufferQueue[i], 0, sizeof(Queue));
		memset(ReceiveQueue[i], 0, sizeof(Queue));
		memset(TransmitQueue[i], 0, sizeof(Queue));
	}

	//create interrupt vector table in kernel
	void **interruptVectorTable =
			 (void **) malloc(sizeof(void *) * TRAP_VECTOR_SIZE);
	if (NULL == interruptVectorTable) {
		TracePrintf(0, "Interrupt Vector Table cannot be initialized!\n");
		return;
	}

	//fill each entry in table with pointer to correct handler
	interruptVectorTable[TRAP_KERNEL] 	= &TrapKernelHandler;
	interruptVectorTable[TRAP_CLOCK] 	= &TrapClockHandler;
	interruptVectorTable[TRAP_ILLEGAL] 	= &TrapIllegalHandler;
	interruptVectorTable[TRAP_MEMORY] 	= &TrapMemoryHandler;
	interruptVectorTable[TRAP_MATH] 	= &TrapMathHandler;
	interruptVectorTable[TRAP_TTY_RECEIVE] 	= &TrapTtyReceiveHandler;
	interruptVectorTable[TRAP_TTY_TRANSMIT]	= &TrapTtyTransmitHandler;
	interruptVectorTable[TRAP_DISK] 	= &TrapDiskHandler;
	for (i = 8; i < 16; i++) {
		interruptVectorTable[i] 	= &TrapErrorHandler;
	}	

	//point REG_VECTOR_BASE to interrupt vector table
	WriteRegister(REG_VECTOR_BASE, (u_int) interruptVectorTable);
	TracePrintf(0, "Interrupt vector table initialization completed.\n");

	//Build page table for Region 0
	KernelPageTable = (PTE *) malloc(sizeof(PTE) * MAX_PT_LEN);
	if (NULL == KernelPageTable) {
		TracePrintf(0, "KernelPageTable cannot be initialized!\n");
		return;
	}
	memset(KernelPageTable, 0, sizeof(PTE) * MAX_PT_LEN);

	TracePrintf(0, "Page tables built successfully.\n");

	//create kernel stack union
	KS = (KernelStack *) KERNEL_STACK_BASE;
	TracePrintf(0, "Kernel stack initialization completed.\n");

	//build input init process PCB
	KS->CurrentPCB = InitPCB = InitializePCB();
	if (NULL == InitPCB) {
		TracePrintf(0, "InitPCB cannot be initialized!\n");
		return;
	}

	// We enablePTE after using all the malloc in the kernel because
	// only in this way we can make sure the getFreeFrame will be called
	// and update the FreeFrameCount.
	enablePTE(0, Page(KernelDataStart), PROT_READ | PROT_EXEC);
	enablePTE(Page(KernelDataStart), Page(KernelBrk), PROT_READ | PROT_WRITE);
	enablePTE(INTERFACE, Page(KERNEL_STACK_LIMIT), PROT_READ | PROT_WRITE);

	// initialize virtual memory registers
	WriteRegister(REG_PTBR0, (u_int) KernelPageTable);
	WriteRegister(REG_PTLR0, MAX_PT_LEN);
	WriteRegister(REG_PTBR1, (u_int) KS->CurrentPCB->pagetable);
	WriteRegister(REG_PTLR1, MAX_PT_LEN);
	TracePrintf(0, "Table information loading to hardware completed.\n");

	//set up the link list to keep track of free frames
	//from KernelBrk to KERNEL_STACK_BASE
	*FrameNumber(INTERFACE) = Page(KernelBrk);
	for (i = Page(KernelBrk); i < INTERFACE - 1; i++) {
		*FrameNumber(i) = i + 1;
	}
	*FrameNumber(INTERFACE - 1) = Page(VMEM_0_LIMIT);
	for (i = Page(VMEM_0_LIMIT); i < Page(pmem_size - 1); i++) {
		*FrameNumber(i) = i + 1;
	}
	TracePrintf(0, "Free frames' linked list has been set up.\n");

	//enable virtual memory
	WriteRegister(REG_VM_ENABLE, 1);
	VM_ENABLED = 1;
	TracePrintf(0, "Virtual memory has been enabled.\n");

	TracePrintf(0, "Load input program. (Default init)\n");
	if (NULL != cmd_args[0]) {
		if (FAILURE == LoadProgram(cmd_args[0], cmd_args, InitPCB)) {
			TracePrintf(0, "Load input program failed!\n");
			return;
		}
	} else {
		char *args[] = {"init", NULL};
		if (FAILURE == LoadProgram(args[0], args, InitPCB)) {
			TracePrintf(0, "Load input program failed!\n");
			return;
		}
	}

	TracePrintf(0, "Build the Idle process.\n");
	//build kernel idle process PCB
	IdlePCB = InitializePCB();
	if (NULL == IdlePCB) {
		TracePrintf(0, "IdlePCB cannot be initialized!\n");
		return;
	}

	//sp = virtual address of top of stack
	IdlePCB->uctxt.sp  = (void *) (VMEM_1_LIMIT
				     - INITIAL_STACK_FRAME_SIZE
				     - POST_ARGV_NULL_SPACE);
	//pc = virtual address of DoIdle function
	IdlePCB->uctxt.pc  = &DoIdle;

	//enable user stack for idle process	
	IdlePCB->pagetable[MAX_PT_LEN - 1].valid = 1;
	IdlePCB->pagetable[MAX_PT_LEN - 1].prot = PROT_READ | PROT_WRITE;

	if (0 == FreeFrameCount) {
		TracePrintf(0, "No frame for Idle process pagetable!\n");
		return;
	}

	IdlePCB->pagetable[MAX_PT_LEN - 1].pfn = getFreeFrame(0);

	//use MyKCS initialize IdlePCB's KC and KS
	KernelContextSwitch(KCKSInit, (void *) IdlePCB, NULL);
	TracePrintf(0, "How many time will this line be printed?\n");

	//copy current context into hardware provided context
	*uctxt = KS->CurrentPCB->uctxt;

	TracePrintf(0, "KernelStart finished.\n");
}

int SetKernelBrk(void *addr) {
	TracePrintf(2, "SetKernelBrk start\n");
	u_int i;
	
	u_int address = (u_int) addr;
	u_int upperAddress = UP_TO_PAGE(address);

	int addressPage = Page(upperAddress);

	// check to make sure address is below the stack
	if (address < KernelDataEnd || address >= Address(INTERFACE)) {
		TracePrintf(0, "ERROR: SetKernelBrk Failure! Bad address!\n");
		return FAILURE;
	}

	// If virtual memory has not been enabled
	if (0 == VM_ENABLED) {
		if (KernelBrk < address)
			KernelBrk = upperAddress; 
	} else {
		if (FAILURE == disablePTE(addressPage, INTERFACE)) {
			TracePrintf(0, "SetKernelBrk Failure, disable pte fail!");
			return FAILURE;
		}
		if (FAILURE == enablePTE(MIN_VPN, addressPage, PROT_READ | PROT_WRITE)) {
			TracePrintf(0, "SetKernelBrk Failure, enable pte fail!");
			return FAILURE;
		}
	}
	TracePrintf(2, "NewBrkPage: %d, Free frames: %d\n", addressPage, FreeFrameCount);
	TracePrintf(2, "SetKernelBrk finished\n");
	return SUCCESS;
}

