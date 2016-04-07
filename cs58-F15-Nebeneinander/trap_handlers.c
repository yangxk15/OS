#include "trap_handlers.h"

void TrapKernelHandler(UserContext *uctxt) {
	char *indent = "Trap_Kernel_Handler:";
	TracePrintf(0, "%s Start!\n", indent);

	TracePrintf(1, "Saving UserContext...\n");
	KS->CurrentPCB->uctxt = *uctxt;

	//get kerne call number from uctxt->code	
	int call_number = uctxt->code;
	TracePrintf(1, "%s call_number = %x\n", indent, call_number);

	//get arguments from uctxt->regs[]
	//lookup kernel call from yalnix.h
	int tempReturn = 0;
	switch (call_number) {
		case YALNIX_FORK:
			tempReturn = KernelFork();
			break;
		case YALNIX_EXEC:
			tempReturn = KernelExec((char *) uctxt->regs[0],
						(char **) uctxt->regs);
			break;
		case YALNIX_EXIT:
			KernelExit(uctxt->regs[0]);
			break;
		case YALNIX_WAIT:
			tempReturn = KernelWait((int *) uctxt->regs[0]);
			break;
		case YALNIX_GETPID:
			tempReturn = KernelGetPid();
			break;
		case YALNIX_BRK:
			tempReturn = KernelUserBrk((void *) uctxt->regs[0]);
			break;
		case YALNIX_DELAY:
			tempReturn = KernelDelay(uctxt->regs[0]);
			break;
		case YALNIX_TTY_READ:
			tempReturn = KernelTtyRead(uctxt->regs[0],
					  (void *) uctxt->regs[1],
						   uctxt->regs[2]);
			break;
		case YALNIX_TTY_WRITE:
			tempReturn = KernelTtyWrite(uctxt->regs[0],
					   (void *) uctxt->regs[1],
						    uctxt->regs[2]);
			break;
		case YALNIX_PIPE_INIT:
			tempReturn = KernelPipeInit((int *) uctxt->regs[0]);
			break;
		case YALNIX_PIPE_READ:
			tempReturn = KernelPipeRead(uctxt->regs[0],
					   (void *) uctxt->regs[1],
						    uctxt->regs[2]);
			break;
		case YALNIX_PIPE_WRITE:
			tempReturn = KernelPipeWrite(uctxt->regs[0],
					   (void *) uctxt->regs[1],
						    uctxt->regs[2]);
			break;
		case YALNIX_LOCK_INIT:
			tempReturn = KernelLockInit((int *) uctxt->regs[0]);
			break;
		case YALNIX_LOCK_ACQUIRE:
			tempReturn = KernelAcquire(uctxt->regs[0]);
			break;
		case YALNIX_LOCK_RELEASE:
			tempReturn = KernelRelease(uctxt->regs[0]);
			break;
		case YALNIX_CVAR_INIT:
			tempReturn = KernelCvarInit((int *) uctxt->regs[0]);
			break;
		case YALNIX_CVAR_SIGNAL:
			tempReturn = KernelCvarSignal(uctxt->regs[0]);
			break;
		case YALNIX_CVAR_BROADCAST:
			tempReturn = KernelCvarBroadcast(uctxt->regs[0]);
			break;
		case YALNIX_CVAR_WAIT:
			tempReturn = KernelCvarWait(uctxt->regs[0],
						    uctxt->regs[1]);
			break;
		case YALNIX_SEM_INIT:
			tempReturn = KernelSemInit((int *) uctxt->regs[0],
						   uctxt->regs[1]);
			break;
		case YALNIX_SEM_UP:
			tempReturn = KernelSemUp(uctxt->regs[0]);
			break;
		case YALNIX_SEM_DOWN:
			tempReturn = KernelSemDown(uctxt->regs[0]);
			break;
		case YALNIX_RECLAIM:
			tempReturn = KernelReclaim(uctxt->regs[0]);
			break;
	}

	TracePrintf(1, "Restoring UserContext...\n");
	*uctxt = KS->CurrentPCB->uctxt;

	uctxt->regs[0] = tempReturn;

	TracePrintf(0, "%s Exit!\n", indent);
}

void TrapClockHandler(UserContext *uctxt) {

	char *indent = "Trap_Clock_Handler:";
	TracePrintf(0, "\n%s Start! (Current process %d)\n",
			indent, KS->CurrentPCB->pid);

	Enqueue(ReadyQueue, &KS->CurrentPCB->queue_node);

	TracePrintf(1, "Saving UserContext...\n");
	KS->CurrentPCB->uctxt = *uctxt;

	if (FAILURE == KCSwitchToQueue(ReadyQueue)) {
		TracePrintf(1, "%s KernelContextSwitch failed!\n", indent);
	} else {
		TracePrintf(1, "%s KernelContextSwitch succeeded!\n", indent);
	}

	TracePrintf(1, "Restoring UserContext...\n");
	*uctxt = KS->CurrentPCB->uctxt;

	//Decrement the Delay Queue
	DecrementDelayQueue();

	TracePrintf(0, "\n%s Exit! (Current process %d)\n",
			indent, KS->CurrentPCB->pid);
}

void TrapIllegalHandler(UserContext *uctxt) {
	char *indent = "Trap_Illegal_Handler:";
	TracePrintf(0, "%s Start!\n", indent);
	//get type of illegal instruction from uctxt->code
	TracePrintf(0, "%s Process %d, error code %d",
			indent, KernelGetPid(), uctxt->code);
	//abort current process
	KernelExit(FAILURE);
	TracePrintf(0, "%s Exit!\n", indent);
}

void TrapMemoryHandler(UserContext *uctxt) {
	char *indent = "Trap_Memory_Handler:";
	TracePrintf(0, "\n%s Start! (Called by process %d)\n",
			indent, KS->CurrentPCB->pid);
	u_int addr = (u_int) uctxt->addr;
	int page = Page(addr);
	
	//check the type of trap: YALNIX_MAPERR or YALNIX_ACCERR
	if (YALNIX_ACCERR == uctxt->code) {
		TracePrintf(0, "%s Invalid permissions to access page %d!\n",
				indent, page);
		TracePrintf(0, "%s Abort the current process!\n", indent);
		KernelExit(FAILURE);
		return;
	}
		
	TracePrintf(1, "%s Trying to access page %d! The address is %d\n",
			indent, page, addr);

	//determine if it is implicit request for stack space
	// is uctxt->addr in region 1? below the stack? above the break?
	//todo: include check for above brk
	int brkPage = Page((int) KS->CurrentPCB->brk);
	if (brkPage > page) {
		TracePrintf(0, "%s Error: Address is below the brk\n", indent);
		KernelExit(FAILURE);
	} else if (brkPage + 1 >= page) {
		TracePrintf(0, "%s Error: Address is too close to the brk\n",
				indent);
		KernelExit(FAILURE);
	}

	if (addr < VMEM_1_LIMIT) {
		if (PTEisValid(page)) {
			TracePrintf(0, "%s Error: TRAP_MEMORY on valid page!\n",
					 indent);
			KernelExit(FAILURE);
		}
		int stackBasePage = Page((int) KS->CurrentPCB->stackBase); 
		int stackPagesNeeded = stackBasePage - page;
		if (FreeFrameCount < stackPagesNeeded) {
			TracePrintf(0, "%s Not enough free frames\n", indent);
			//page to disk, but for now:
			KernelExit(FAILURE);
		}
		//allocate the appropriate pages
		u_int i;
		if (FAILURE == enablePTE(page, stackBasePage,
					 PROT_READ | PROT_WRITE)) {
			TracePrintf(0, "%s Error: Failed to enable PTE %d\n",
					indent, i);
			KernelExit(FAILURE);
		}
		TracePrintf(1, "%s Stack grew by %d page(s)\n", 
				indent, stackPagesNeeded);
		KS->CurrentPCB->stackBase = (void *) Address(page);
	} else {
		TracePrintf(0, "%s Address above address space limit!\n");
		TracePrintf(0, "%s Abort the current process!\n", indent);
		KernelExit(FAILURE);
	}
	TracePrintf(0, "\n%s Exit! (Called by process %d)\n",
			indent, KS->CurrentPCB->pid);
}

void TrapMathHandler(UserContext *uctxt) {
	char *indent = "Trap_Math_Handler:";
	TracePrintf(0, "%s Start!\n", indent);
	//get type of math error from uctxt->code
	TracePrintf(0, "%s Process %d, error code %d",
			indent, KernelGetPid(), uctxt->code);
	//switch context to next process from ready queue
	KernelExit(FAILURE);
	TracePrintf(0, "%s Exit!\n\n", indent);
}

/* Please see TTY DATA STRUCTURES section of README */
void TrapTtyReceiveHandler(UserContext *uctxt) {
	char *indent = "Trap_Tty_Receive_Handler:";
	TracePrintf(0, "%s Start!\n", indent);
	//get terminal number from uctxt->code
	int ttyid = uctxt->code;
	//get a buffer struct
	TtyBuffer *ttyBuf = (TtyBuffer *) malloc(sizeof(TtyBuffer));
	if (NULL == ttyBuf) {
		TracePrintf(0, "%s Failed to malloc buffer\n", indent);
	}
	memset(ttyBuf, 0, sizeof(TtyBuffer));

	//receieve input to buf and return length to struct -> len
	ttyBuf->len = TtyReceive(ttyid, ttyBuf->buf, TERMINAL_MAX_LINE);

	//place buffer in queue for next read to collect
	// and switch to receiving process if it exists
	Enqueue(BufferQueue[ttyid], &ttyBuf->node);
	if (FAILURE == QueueIsEmpty(ReceiveQueue[ttyid])) {
		Enqueue(ReadyQueue, &KS->CurrentPCB->queue_node);
		KCSwitchToNode(ReceiveQueue[ttyid]->head);
	}
	TracePrintf(0, "%s Exit!\n\n", indent);
}

/* Please see TTY DATA STRUCTURES section of README */
void TrapTtyTransmitHandler(UserContext *uctxt) {
	char *indent = "Trap_Tty_Transmit_Handler:";
	TracePrintf(0, "%s Start!\n", indent);
	//get terminal number from uctxt->code
	int ttyid = uctxt->code;
	//switch to process that transmitted to this terminal 
	Enqueue(ReadyQueue, &KS->CurrentPCB->queue_node);
	KCSwitchToNode(TransmitQueue[ttyid]->head);
	TracePrintf(0, "%s Exit!\n\n", indent);
}

void TrapDiskHandler(UserContext *uctxt) {
	char *indent = "Trap_Disk_Handler:";
	TracePrintf(0, "%s Start!\n", indent);
	//No mention of this in yalnix manual
	TracePrintf(0, "%s Process %d, code %d",
			indent, KernelGetPid(), uctxt->code);
	TracePrintf(0, "%s Exit!\n\n", indent);
}

void TrapErrorHandler(UserContext *uctxt) {
	char *indent = "Trap_Error_Handler:";
	TracePrintf(0, "%s Start!\n", indent);
	TracePrintf(0, "%s Process %d, error code %d",
			indent, KernelGetPid(), uctxt->code);
	KernelExit(FAILURE);
	//Report error, undefined trap
	TracePrintf(0, "%s Exit!\n\n", indent);
}
