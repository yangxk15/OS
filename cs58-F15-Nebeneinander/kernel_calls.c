#include "kernel_calls.h"

/** Kernel Calls Helper: Check Address
 *	When a user process makes a kernel call
 *	in which it passes an address (a pointer),
 *	the kernel fisrt check if this address (or section)
 *	is in the userland. Return ERROR if it points to
 *	somewhere in the kernel region or anywhere else
 *	Otherwise return SUCCESS normally.
 */
int CheckAddress(void *address, int len) {
	if (0 == len) {
		//do not have to check address because no address will be accessed
		return SUCCESS;
	}
	if (0 > len) {
		//len is negative indicating that input is illegal
		return ERROR;
	}
		
	u_int addr = (u_int) address;
	if (addr < VMEM_0_LIMIT || addr + len - 1 >= VMEM_1_LIMIT 
			|| 0 == PTEisValid(Page(addr))
			|| 0 == PTEisValid(Page(addr + len - 1)))
		{
		TracePrintf(0, "Error: Illegal user input address: %x\n", addr);
		return ERROR;
	}
	return SUCCESS;
}

int KernelFork() {
	char *indent = "KernelFork:";
	TracePrintf(0, "%s Start!\n", indent);

	int page_in_use = 	(Page(KS->CurrentPCB->brk) - MAX_PT_LEN) 
			      + (NUM_VPN - Page(KS->CurrentPCB->stackBase));
	if (page_in_use + KERNEL_STACK_FRAMES > FreeFrameCount) {
		TracePrintf(0, "There is not enough free frames to fork!\n");
		return ERROR;
	}

	PCB *child = InitializePCB();
	if (NULL == child) {
		TracePrintf(1, "Forking child process falied! \
				Initialization failed!\n");
		return ERROR;
	}

	u_int i;
	//mapping the pagetable using INTERFACE PAGE
	//see PHYSICAL MEMORY MANAGEMENT section of README for more info 
	for (i = 0; i < MAX_PT_LEN; i++) {
		if (0 == PTEisValid(MAX_PT_LEN + i))
			continue;
		int t = *FrameNumber(INTERFACE);
		memcpy(Address(INTERFACE), Address(MAX_PT_LEN + i), PAGESIZE);
		child->pagetable[i].valid = 1;
		child->pagetable[i].prot = KS->CurrentPCB->pagetable[i].prot;
		child->pagetable[i].pfn = getFreeFrame(0);
		changePTE(INTERFACE, -1, t);
	}

	//initialize the user context
	child->uctxt = KS->CurrentPCB->uctxt;
	//copy the brk stack data address
	child->stackBase = KS->CurrentPCB->stackBase;
	child->brk = KS->CurrentPCB->brk;
	child->userDataEnd = KS->CurrentPCB->userDataEnd;

	//connect child and parent
	//child -> parent
	child->parent = KS->CurrentPCB;
	//parent -> child
	InsertPCB(child);

	int childpid = child->pid;

	if (FAILURE == KernelContextSwitch(KCKSInit, child, NULL)) {
		TracePrintf(0, "%s Error: ContextSwtich\n", indent);
		return ERROR;
	}
	TracePrintf(0, "%s Exit!\n", indent);
	if (child->pid == KS->CurrentPCB->pid)
		return 0;
	return childpid;
}

int KernelExec(char *program, char **args) {
	char *indent = "KernelExec:";
	TracePrintf(0, "%s Start!\n", indent);
	int rc = LoadProgram(program, args, KS->CurrentPCB);
	if (FAILURE == rc) {
		TracePrintf(1, "Load Program Failed!\n");
		return ERROR;
	} else if (KILL == rc) {
		TracePrintf(1, "Load Program Failed! Kill the current process!\n");
		KernelExit(-1);
	}
	TracePrintf(0, "%s Exit!\n", indent);
	return SUCCESS;
}

/* Please see EXIT / WAIT section in README */
void KernelExit(int status) {
	char *indent = "KernelExit:";
	TracePrintf(0, "%s Start!\n", indent);
	//if the initial process exits, you should halt the system.
	int pid = KernelGetPid();
	if (1 == pid) {
		TracePrintf(0, "%s Exit from Initial function!\n", indent);
		Halt();
	}

	//This should never happen!
	if (NULL != KS->CurrentPCB->queue_node.next) {
		TracePrintf(0, "%s This process's death will \
				corrupt some queue\n", indent);
	} 
	

	//if the parent process is still alive
	//exit status is added to a FIFO queue of child processes
	// not yet collected by its specific parent
	if(NULL != KS->CurrentPCB->parent) {
		ExitStatus *thisStatus = (ExitStatus *) malloc(sizeof(ExitStatus));
		if (NULL != thisStatus) {
			thisStatus->pid 	= pid;
			thisStatus->status	= status;
			thisStatus->queue_node.next 	= NULL;
			PCB *parent = KS->CurrentPCB->parent;
			Enqueue(&parent->childExitStatus, &thisStatus->queue_node);

			if (KS->CurrentPCB->parent->waitingOnChild) {
				KS->CurrentPCB->parent->waitingOnChild = 0;
				Enqueue(ReadyQueue, &KS->CurrentPCB->parent->queue_node);
			}
		}
	}
	TracePrintf(0, "%s Exit!\n", indent);
	//KCKSFree will free PCB and all process-specific frames
	KernelContextSwitch(KCKSFree,
		   (void *) KS->CurrentPCB,
		   (void *) containerOf(Dequeue(ReadyQueue), PCB, queue_node));
}

/* Please see EXIT / WAIT section in README */
int KernelWait(int *status_ptr) {
	char *indent = "KernelWait:";
	TracePrintf(0, "%s Start!\n", indent);

	if (ERROR == CheckAddress((void *) status_ptr, 1)) {
		TracePrintf(0, "%s Exit!\n", indent);
		return ERROR;
	}

	//check whether there is a child or zombie child status
	if (NULL == KS->CurrentPCB->child &&
	    NULL == KS->CurrentPCB->childExitStatus.head) {
		TracePrintf(0, "%s Error: Nothing to wait for\n", indent);
		return ERROR;
	} 
	//If no exited child processes, wait for child
	if (NULL == KS->CurrentPCB->childExitStatus.head) {
		TracePrintf(1, "%s Waiting on child\n", indent);
		KS->CurrentPCB->waitingOnChild = WAITING;
		if (FAILURE == KCSwitchToQueue(ReadyQueue)) {
			TracePrintf(0, "%s Failure, Bad context switch\n",
					indent);
			return ERROR;
		}
		TracePrintf(1, "%s Returning from wait\n", indent);
	}
	//Collect the child process and exit status;
	ExitStatus *thisStatus = 
		containerOf(Dequeue(&KS->CurrentPCB->childExitStatus),
			    ExitStatus, queue_node);
	int child_pid = thisStatus->pid;
	*status_ptr = thisStatus->status;

	free(thisStatus);
	TracePrintf(0, "%s Exit!\n", indent);
	return child_pid;
}

int KernelGetPid() {
	char *indent = "KernelGetPid:";
	TracePrintf(0, "%s Called!\n", indent);
	//return pid from current_pcb_pointer
	return KS->CurrentPCB->pid;
}

int KernelUserBrk(void * addr) {
	char *indent = "KernelUserBrk:";
	TracePrintf(0, "%s Start!\n", indent);
	//check if the input address if valid
	TracePrintf(1, "%s New Address: 0x%x\n", indent, addr);
	TracePrintf(1, "%s Old Address: 0x%x\n", indent, KS->CurrentPCB->brk);
	if (addr <= KS->CurrentPCB->userDataEnd || addr >= (void *) VMEM_1_LIMIT) {
		TracePrintf(0, "%s Error: Invalid address\n", indent);
		return ERROR;
	}	

	void *newBrk = (void *) UP_TO_PAGE(addr);
	int newBrkPage = Page((u_int) newBrk);
	int curBrkPage = Page((u_int) KS->CurrentPCB->brk);

	int i;
	if (addr < KS->CurrentPCB->brk) {
		//disable pages- start: new brk; end: old brk
		if (FAILURE == disablePTE(newBrkPage, curBrkPage)) {
			return ERROR;
		}
		TracePrintf(1, "%s Heap decreased by %d page(s)\n",
				indent,
				curBrkPage - newBrkPage);
	} else {
		if (PTEisValid(newBrkPage)) {
			TracePrintf(0, "%s Error: address in stack\n", indent);
			return ERROR;
		}
		if (PTEisValid(newBrkPage + 1)) {
			TracePrintf(0, "%s Error: address too close to stack\n",
					indent);
			return ERROR;
		}
		//enable pages - start: below new brk; end: old brk
		int rc = enablePTE(curBrkPage, newBrkPage,
				   PROT_READ | PROT_WRITE);
		if (rc == FAILURE) {
			TracePrintf(0,
				"%s Error: Failed to enable heap page %d\n",
				indent, i);
			return ERROR;
		}
		TracePrintf(1, "%s Heap increased by %d page(s)\n",
				indent,
				newBrkPage - curBrkPage);

	}
	KS->CurrentPCB->brk = newBrk;
	TracePrintf(0, "%s Exit!\n", indent);
	return 0;
}

int KernelDelay(int clock_ticks_to_wait) {
	char *indent = "KernelDelay:";
	TracePrintf(0, "%s Start!\n", indent);

	if (clock_ticks_to_wait < 0) {
		TracePrintf(0, "%s Error, Negative input\n", indent);
		return ERROR;
	}
	if (clock_ticks_to_wait == 0) {
		TracePrintf(0, "%s Zero delay, immediate return\n", indent);
		return SUCCESS;
	}

	TracePrintf(1, "%s Enqueuing in DelayQueue\n", indent);
	Enqueue(DelayQueue, &KS->CurrentPCB->queue_node);

	KS->CurrentPCB->timer = clock_ticks_to_wait;

	if (FAILURE == KCSwitchToQueue(ReadyQueue)) {
		TracePrintf(0, "%s Failure, Bad context switch\n", indent);
		return ERROR;
	}
	TracePrintf(0, "%s Exit!\n", indent);
	return SUCCESS;
}

/* Please see TTY DATA STRUCTURES section of README */
int KernelTtyRead(int ttyid, void *buf, int len) {
	char *indent = "KernelTtyRead:";
	TracePrintf(0, "%s Start!\n", indent);

	if (ttyid > 4 || ttyid < 0) {
		TracePrintf(0, "%s Illegal terminal id!\n", indent);
		TracePrintf(0, "%s Exit!\n", indent);
		return ERROR;
	}

	if (ERROR == CheckAddress(buf, len)) {
		TracePrintf(0, "%s Exit!\n", indent);
		return ERROR;
	}

	if (len < 0) {
		return ERROR;
	}

	//only the head of the receiving queue can read the buffer,
	// so enqueue
	Enqueue(ReceiveQueue[ttyid], &KS->CurrentPCB->queue_node);
	//block if there is nothing to read or not head of queue
	if (SUCCESS == QueueIsEmpty(BufferQueue[ttyid]) ||
	    &KS->CurrentPCB->queue_node != ReceiveQueue[ttyid]->head) {
		TracePrintf(0, "%s Block the current process!\n", indent);
		if (FAILURE == KCSwitchToQueue(ReadyQueue)) {
			return ERROR;
		}
	}
		
	//get first unread line of input from buffer queue
	//see data_structures.h and README for more info on TtyBuffer

	// actual_len will eventually be total length of returned buffer
	int actual_len = 0; 
	int len_left_to_fill = len; // how much of the buffer is still empty

	//while (the current line has not been fully read into the buffer
	//or the buffer has not been filled)
	int get_next_buffer_flag = 1;
	while (get_next_buffer_flag) {
		TtyBuffer *ttyBuf = containerOf(BufferQueue[ttyid]->head,
						TtyBuffer, node);
		
		int current_buf_len = ttyBuf->len - ttyBuf->start;
		if (current_buf_len >= len_left_to_fill) {
			//input buffer >= output buffer
			TracePrintf(2, "%s Case 1: input longer or equal to \
					remaining buffer\n", indent);

			//fill the output buffer
			memcpy(	buf + actual_len,
				ttyBuf->buf + ttyBuf->start,
				len_left_to_fill);

			//move start of input buffer, update len
			ttyBuf->start += len_left_to_fill;
			actual_len += len_left_to_fill;

			//output buffer full, do not loop
			get_next_buffer_flag = 0;
		} else {
			//input buffer < output buffer
			TracePrintf(2, "%s Case 2: input shorter than \
					remaining buffer\n", indent);
			//copy to output buffer
			memcpy(	buf + actual_len,
				ttyBuf->buf + ttyBuf->start,
				current_buf_len);
			actual_len += current_buf_len;

			//dequeue and free the buffer
			free(containerOf(Dequeue(BufferQueue[ttyid]),
					 TtyBuffer, node));

			//if (output buffer ends in newline 
			//or '^d' was typed)
			//	do not loop
			//else
			//	did not end in newline, did not reach max len
			// 	block and wait for next buffer,
			// 	or loop right away if already received
			if ('\n' == ((char *) buf)[actual_len - 1]
					|| 0 == current_buf_len) {
				TracePrintf(2, "%s Full line has been read\n", indent);
				get_next_buffer_flag = 0;
			} else if (SUCCESS == QueueIsEmpty(BufferQueue[ttyid])) {
				TracePrintf(0, "%s Full line not read yet. \
						Block the current process!\n",
						indent);
				if (FAILURE == KCSwitchToQueue(ReadyQueue)) {
					TracePrintf(0, "%s Failure, \
							Bad context switch\n",
							indent);
					return ERROR;
				}
			}
		}
	}
	//finished copying to output buffer, so dequeue from receive queue
	Dequeue(ReceiveQueue[ttyid]);
	//if there are more buffers to read and
	// more processes waiting to read them
	// switch to the next process in the receive queue
	if (FAILURE == QueueIsEmpty(BufferQueue[ttyid])
	 && FAILURE == QueueIsEmpty(ReceiveQueue[ttyid])) {
		Enqueue(ReadyQueue, &KS->CurrentPCB->queue_node);
		if (FAILURE == KCSwitchToNode(ReceiveQueue[ttyid]->head)) {
			return ERROR;
		}
	}

	TracePrintf(0, "%s Exit!\n", indent);
	return actual_len;
}

/* Please see TTY DATA STRUCTURES section of README */
int KernelTtyWrite(int ttyid, void *buf, int len) {
	char *indent = "KernelTtyWrite:";
	TracePrintf(0, "%s Start!\n", indent);

	if (ttyid > 4 || ttyid < 0) {
		TracePrintf(0, "%s Wrong terminal id!\n", indent);
		TracePrintf(0, "%s Exit!\n", indent);
		return ERROR;
	}

	if (ERROR == CheckAddress(buf, len)) {
		TracePrintf(0, "%s Exit!\n", indent);
		return ERROR;
	}

	if (len < 0) {
		return ERROR;
	}

	char *temp = (char *) malloc(sizeof(char) * len);
	if (NULL == temp) {
		return ERROR;
	}
	TracePrintf(0, "I got here!\n");
	memcpy(temp, buf, len);
	TracePrintf(0, "I got here, too!\n");

	int i = 0;
	//if there is a process currently writing to terminal ttyid
	Enqueue(TransmitQueue[ttyid], &KS->CurrentPCB->queue_node);
	TracePrintf(0, "I got here, three!\n");
	if (&KS->CurrentPCB->queue_node != TransmitQueue[ttyid]->head) {
		TracePrintf(1, "%s Blocking because there are others \
				ahead of me\n", indent);
		if (FAILURE == KCSwitchToQueue(ReadyQueue)) {
			return ERROR;
		}
	}
	do {
		TtyTransmit(ttyid, temp + i, 
			TERMINAL_MAX_LINE < len - i ?
			TERMINAL_MAX_LINE : len - i);
		i += TERMINAL_MAX_LINE; 
		TracePrintf(1, "%s Block while waiting for TrapTtyTransmit\n",
				indent);
		if (FAILURE == KCSwitchToQueue(ReadyQueue)) {
			return ERROR;
		}
	} while (i < len);
	TracePrintf(1, "%s Finished transmission\n", indent);
	free(temp);
	Dequeue(TransmitQueue[ttyid]);
	if (FAILURE == QueueIsEmpty(TransmitQueue[ttyid])) {
		TracePrintf(1, "%s Switching to next transmitter\n", indent);
		Enqueue(ReadyQueue, &KS->CurrentPCB->queue_node);
		if (FAILURE == KCSwitchToNode(TransmitQueue[ttyid]->head)) {
			return ERROR;
		}
	}
		
	TracePrintf(0, "%s Exit!\n", indent);
}

/* Please see IPC TABLE STRUCTURE: Pipe section of README */
int KernelPipeInit(int *pipe_idp) {
	char *indent = "KernelPipeInit:";
	TracePrintf(0, "%s Start!\n", indent);
	
	if (ERROR == CheckAddress((void *) pipe_idp, 1)) {
		TracePrintf(0, "%s Exit!\n", indent);
		return ERROR;
	}

	Pipe *thisPipe = (Pipe *) malloc(sizeof(Pipe));
	if (NULL == thisPipe) {
		TracePrintf(0, "Pipe malloc failed!\n");
		return ERROR;
	}

	memset(thisPipe, 0, sizeof(Pipe));
	GetNewID(PIPE, &thisPipe->object_node);

	*pipe_idp = thisPipe->object_node.id;

	TracePrintf(0, "%s Exit!\n", indent);
	return SUCCESS;
}

/* Memory Copy by Round for KernelPipeRead
 *	Since we use two integer pointer to record
 *	where the current start index and end index are,
 *	we use this memcpy_round_read to copy the memory
 *	from src to des for len bytes. The address src
 *	has only PIPE_MAX_LENGTH bytes to copy, thus we
 *	need to copy twice to avoid address voilation if
 *	the bytes available in src is less than ths len
 *	we are going to copy.
 */
void *memcpy_round_read(void *des, void *src, int start, int len) {
	int start_left = PIPE_MAX_LENGTH - start;
	if (len <= start_left) {
		memcpy(des, src + start, len);
	} else {
		memcpy(des, src + start, start_left);
		memcpy(des + start_left, src, len - start_left);
	}
	return des + len;
}

/* Memory Copy by Round for KernelPipeWrite
 *	Since we use two integer pointer to record
 *	where the current start index and end index are,
 *	we use this memcpy_round_write to copy the memory
 *	from src to des for len bytes. The address des 
 *	has only PIPE_MAX_LENGTH bytes to copy, thus we
 *	need to copy twice to avoid address voilation if
 *	the bytes available in des is less than ths len
 *	we are going to copy.
 */
void *memcpy_round_write(void *des, void *src, int start, int len) {
	int start_left = PIPE_MAX_LENGTH - start;
	if (len <= start_left) {
		memcpy(des + start, src, len);
	} else {
		memcpy(des + start, src, start_left);
		memcpy(des, src + start_left, len - start_left);
	}
	return src + len;
}

/* Please see IPC TABLE STRUCTURE: Pipe section of README */
int KernelPipeRead(int pipe_id, void *buf, int len) {
	char *indent = "KernelPipeRead:";
	TracePrintf(0, "%s Start!\n", indent);

	if (pipe_id < 0) {
		TracePrintf(0, "%s ERROR: Invalid negative ID\n", indent);
		return ERROR;
	}
	if (len < 0) {
		return ERROR;
	}

	if (ERROR == CheckAddress(buf, len)) {
		TracePrintf(0, "%s Exit!\n", indent);
		return ERROR;
	}

	ObjectNode *object_node = Find(pipe_id);
	if (NULL == object_node) {
		TracePrintf(0, "%s Error: no such pipe!\n", indent);
		return ERROR;
	}

	if (0 == len) {
		TracePrintf(0, "%s Exit!\n", indent);
		return len;
	}

	Pipe *thisPipe = containerOf(object_node, Pipe, object_node);
	Enqueue(&thisPipe->read_queue, &KS->CurrentPCB->queue_node);


	u_int i;
	int left_len = len;
	while (left_len > 0) { //while we still want to read more bytes
		int len_to_copy = thisPipe->end - thisPipe->start;

		// if we will be satisfied by the available bytes
		if (left_len <= len_to_copy) {
			memcpy_round_read(buf, thisPipe->buf, 
					  thisPipe->start % PIPE_MAX_LENGTH,
					  left_len);
			thisPipe->start += left_len;
			break;
		}

		buf = memcpy_round_read(buf, thisPipe->buf, 
					thisPipe->start % PIPE_MAX_LENGTH,
					len_to_copy);
		left_len -= len_to_copy;
		thisPipe->start = thisPipe->end;

		//we still want to read more bytes, so
		//block and switch
		//to write_queue if there is waiting writer
		//or to ready queue if there isn't
		if (FAILURE == QueueIsEmpty(&thisPipe->write_queue)) {
			if (FAILURE == KCSwitchToNode(thisPipe->
							write_queue.head)) {
				TracePrintf(0, "%s Failure, Bad context switch\n", indent);
				return ERROR;
			}
		} else {
			if (FAILURE == KCSwitchToQueue(ReadyQueue)) {
				TracePrintf(0, "%s Failure, Bad context switch\n", indent);
				return ERROR;
			}
		}
		if (NULL == Find(pipe_id)) {
			TracePrintf(0, "%s Error: Pipe reclaimed before \
					read finished\n", indent);
			return ERROR;
		}
	}
	Dequeue(&thisPipe->read_queue);
	//check if there are writers waiting
	if (FAILURE == QueueIsEmpty(&thisPipe->write_queue)) {
		Enqueue(ReadyQueue, &KS->CurrentPCB->queue_node);
		if (FAILURE == KCSwitchToNode(thisPipe->write_queue.head)) {
			TracePrintf(0, "%s Failure, Bad context switch\n", indent);
			return ERROR;
		}
	} else if (FAILURE == QueueIsEmpty(&thisPipe->read_queue)) {
		//no writers are waiting, so switch to next reader
		Enqueue(ReadyQueue, &KS->CurrentPCB->queue_node);
		if (FAILURE == KCSwitchToNode(thisPipe->read_queue.head)) {
			TracePrintf(0, "%s Failure, Bad context switch\n", indent);
			return ERROR;
		}
	}
	TracePrintf(0, "%s Exit!\n", indent);
	return len;
}

/* Please see IPC TABLE STRUCTURE: Pipe section of README */
int KernelPipeWrite(int pipe_id, void *buf, int len) {
	char *indent = "KernelPipeWrite:";
	TracePrintf(0, "%s Start!\n", indent);

	if (pipe_id < 0) {
		TracePrintf(0, "%s ERROR: Invalid negative ID\n", indent);
		return ERROR;
	}
	if (ERROR == CheckAddress(buf, len)) {
		TracePrintf(0, "%s Exit!\n", indent);
		return ERROR;
	}

	if (len < 0) {
		return ERROR;
	}

	if (0 == len) {
		TracePrintf(0, "%s Exit!\n", indent);
		return len;
	}

	ObjectNode *object_node = Find(pipe_id);
	if (NULL == object_node) {
		TracePrintf(0, "%s Error: no such pipe!\n", indent);
		return ERROR;
	}

	Pipe *thisPipe = containerOf(object_node, Pipe, object_node);
	Enqueue(&thisPipe->write_queue, &KS->CurrentPCB->queue_node);

	u_int i;
	int left_len = len;
	while (left_len > 0) { //while we still want to write more bytes
		int pipe_left_len = PIPE_MAX_LENGTH 
				  - (thisPipe->end - thisPipe->start);

		// if we will be satisfied by the available bytes
		if (left_len <= pipe_left_len) {
			memcpy_round_write(thisPipe->buf, buf, 
					   thisPipe->end % PIPE_MAX_LENGTH,
					   left_len);
			thisPipe->end += left_len;
			break;
		}

		buf = memcpy_round_write(thisPipe->buf, buf,
					 thisPipe->end % PIPE_MAX_LENGTH,
					 pipe_left_len);
		left_len -= pipe_left_len;
		thisPipe->end += pipe_left_len;

		//we still want to write more bytes, so
		//block and switch
		//to read_queue if there is waiting reader 
		//or to ready queue if there isn't
		if (FAILURE == QueueIsEmpty(&thisPipe->read_queue)) {
			if (FAILURE == KCSwitchToNode(thisPipe->
							read_queue.head)) {
				TracePrintf(0, "%s Failure, Bad context switch\n", indent);
				return ERROR;
			}
		} else {
			if (FAILURE == KCSwitchToQueue(ReadyQueue)) {
				TracePrintf(0, "%s Failure, Bad context switch\n", indent);
				return ERROR;
			}
		}
		if (NULL == Find(pipe_id)) {
			TracePrintf(0, "%s Error: Pipe reclaimed before \
					write finished\n", indent);
			return ERROR;
		}
	}
	Dequeue(&thisPipe->write_queue);
	//check if there are readers waiting
	if (FAILURE == QueueIsEmpty(&thisPipe->read_queue)) {
		Enqueue(ReadyQueue, &KS->CurrentPCB->queue_node);
		if (FAILURE == KCSwitchToNode(thisPipe->read_queue.head)) {
			TracePrintf(0, "%s Failure, Bad context switch\n",
					indent);
			return ERROR;
		}
	} else if (FAILURE == QueueIsEmpty(&thisPipe->write_queue)) {
		//no writers are waiting, so switch to next reader
		Enqueue(ReadyQueue, &KS->CurrentPCB->queue_node);
		if (FAILURE == KCSwitchToNode(thisPipe->write_queue.head)) {
			TracePrintf(0, "%s Failure, Bad context switch\n",
					indent);
			return ERROR;
		}
	}
	
	TracePrintf(0, "%s Exit!\n", indent);
	return len;
}

int KernelLockInit(int *lock_idp) {
	char *indent = "KernelLockInit:";
	TracePrintf(0, "%s Start!\n", indent);

	if (ERROR == CheckAddress((void *) lock_idp, 1)) {
		TracePrintf(0, "%s Exit!\n", indent);
		return ERROR;
	}

	Lock *thisLock = (Lock *) malloc(sizeof(Lock));
	if (NULL == thisLock) {
		TracePrintf(0, "%s Allocation Error!\n", indent);
		return ERROR;
	}
	memset(thisLock, 0, sizeof(Lock));
	GetNewID(LOCK, &thisLock->object_node);	
	*lock_idp = thisLock->object_node.id;
	thisLock->holder_pid = -1;
	thisLock->state = IDLE;
	TracePrintf(0, "%s Exit!\n", indent);
	return SUCCESS;
}

int KernelAcquire(int lock_id) {
	char *indent = "KernelAcquire:";
	TracePrintf(0, "%s Start!\n", indent);
	if (lock_id < 0) {
		TracePrintf(0, "%s ERROR: Invalid negative ID\n", indent);
		return ERROR;
	}
	ObjectNode *object_node = Find(lock_id);
	if (NULL == object_node) {
		TracePrintf(0, "%s Error: no such Lock!\n", indent);
		return ERROR;
	}
	Lock *thisLock = containerOf(object_node, Lock, object_node);
	while (thisLock->state == BUSY) {
		if (KS->CurrentPCB->pid == thisLock->holder_pid) {
			TracePrintf(0, "%s Process already owns lock\n", indent);
			return ERROR;
		}
		TracePrintf(2, "%s Waiting for lock\n", indent);
		Enqueue(&thisLock->acquire_queue, &KS->CurrentPCB->queue_node);
		if (FAILURE == KCSwitchToQueue(ReadyQueue)) {
			TracePrintf(0, "%s Failure, Bad context switch\n", indent);
			return ERROR;
		}
		if (NULL == Find(lock_id)) {
			TracePrintf(0, "%s Error: Lock reclaimed before acquire complete\n", indent);
			return ERROR;
		}
	}
	thisLock->holder_pid = KS->CurrentPCB->pid;
	thisLock->state = BUSY;
	TracePrintf(0, "%s Exit!\n", indent);
	return SUCCESS;
}

int KernelRelease(int lock_id) {
	char *indent = "KernelRelease:";
	TracePrintf(0, "%s Start!\n", indent);
	if (lock_id < 0) {
		TracePrintf(0, "%s ERROR: Invalid negative ID\n", indent);
		return ERROR;
	}
	ObjectNode *object_node = Find(lock_id);
	if (NULL == object_node) {
		TracePrintf(0, "%s Error: no such Lock!\n", indent);
		return ERROR;
	}
	Lock *thisLock = containerOf(object_node, Lock, object_node);
	
	if (thisLock->holder_pid == KS->CurrentPCB->pid) {
		thisLock->state = IDLE;
		thisLock->holder_pid = -1;
	} else {
		TracePrintf(0, "%s ERROR, lock not owned!\n", indent);
		return ERROR;
	}
	if (FAILURE == QueueIsEmpty(&thisLock->acquire_queue)) {
		Enqueue(ReadyQueue, Dequeue(&thisLock->acquire_queue));
	}
	TracePrintf(0, "%s Exit!\n", indent);
	return SUCCESS;
}

int KernelCvarInit(int *cvar_idp) {
	char *indent = "KernelCvarInit:";
	TracePrintf(0, "%s Start!\n", indent);
	
	if (ERROR == CheckAddress((void *) cvar_idp, 1)) {
		TracePrintf(0, "%s Exit!\n", indent);
		return ERROR;
	}

	Cvar *thisCvar = (Cvar *) malloc(sizeof(Cvar));
	if (thisCvar == NULL) {
		TracePrintf(0, "%s Allocation Error!\n", indent);
		return ERROR;
	}
	memset(thisCvar, 0, sizeof(Cvar));
	GetNewID(CVAR, &thisCvar->object_node);
	*cvar_idp = thisCvar->object_node.id;
	TracePrintf(0, "%s Exit!\n", indent);
	return SUCCESS;
}

int KernelCvarSignal(int cvar_id) {
	char *indent = "KernelCvarSignal:";
	TracePrintf(0, "%s Start!\n", indent);
	if (cvar_id < 0) {
		TracePrintf(0, "%s ERROR: Invalid negative ID\n", indent);
		return ERROR;
	}
	//get cvar pointer from id
	ObjectNode *object_node = Find(cvar_id);
	if (NULL == object_node) {
		TracePrintf(0, "%s Error: no such Cvar!\n", indent);
		return ERROR;
	}
	Cvar *thisCvar = containerOf(object_node, Cvar, object_node);
	
	//put first process from cvar queue in readyqueue
	if (FAILURE == QueueIsEmpty(&thisCvar->wait_queue)) {
		Enqueue(ReadyQueue, Dequeue(&thisCvar->wait_queue));
	}
	TracePrintf(0, "%s Exit!\n", indent);
	return SUCCESS;
}

int KernelCvarBroadcast(int cvar_id) {
	char *indent = "KernelCvarBroadcast:";
	TracePrintf(0, "%s Start!\n", indent);
	//get cvar wait queue address from cvar table
	if (cvar_id < 0) {
		TracePrintf(0, "%s ERROR: Invalid negative ID\n", indent);
		return ERROR;
	}
	ObjectNode *object_node = Find(cvar_id);
	if (NULL == object_node) {
		TracePrintf(0, "%s Error: no such Cvar!\n", indent);
		return ERROR;
	}
	Cvar *thisCvar = containerOf(object_node, Cvar, object_node);

	//Dequeue all nodes in thisCvar's queue
	//and enqueue in readyqueue
	while (FAILURE == QueueIsEmpty(&thisCvar->wait_queue)) {
		Enqueue(ReadyQueue, Dequeue(&thisCvar->wait_queue));
	}
	TracePrintf(0, "%s Exit!\n", indent);
	return SUCCESS;
}

int KernelCvarWait(int cvar_id, int lock_id) {
	char *indent = "KernelCvarWait:";
	TracePrintf(0, "%s Start!\n", indent);
	if (cvar_id < 0) {
		TracePrintf(0, "%s ERROR: Invalid negative ID\n", indent);
		return ERROR;
	}
	//get cvar address from cvar table
	ObjectNode *object_node = Find(cvar_id);
	if (NULL == object_node) {
		TracePrintf(0, "%s Error: no such Cvar!\n", indent);
		return ERROR;
	}
	Cvar *thisCvar = containerOf(object_node, Cvar, object_node);

	//enqueue current process pcb pointer in cvar wait queue
	Enqueue(&thisCvar->wait_queue, &KS->CurrentPCB->queue_node); 
	TracePrintf(2, "%s Releasing lock and blocking\n", indent);
	int rc = KernelRelease(lock_id);
	if (SUCCESS != rc) {
		TracePrintf(0, "%s Lock release error\n", indent);
	}
	if (FAILURE == KCSwitchToQueue(ReadyQueue)) {
		TracePrintf(0, "%s Failure, Bad context switch\n", indent);
		return ERROR;
	}
	if (NULL == Find(cvar_id)) {
		TracePrintf(0, "%s Error: Cvar reclaimed while still waiting\n",
				indent);
		return ERROR;
	}
	TracePrintf(2, "%s Re-acquiring lock\n", indent);
	rc = KernelAcquire(lock_id);
	if (SUCCESS != rc) {
		TracePrintf(0, "%s Lock acquire error\n", indent);
	}
	TracePrintf(0, "%s Exit!\n", indent);
	return SUCCESS;
}

/* Initialize Semaphore
 *	sem_idp:	pointer to userland int, place to store id
 *	start_val:	initial number of 'flags'
 */
int KernelSemInit(int *sem_idp, int start_val) {
	char *indent = "KernelSemInit:";
	TracePrintf(0, "%s Start!\n", indent);
	
	if (ERROR == CheckAddress((void *) sem_idp, 1)) {
		TracePrintf(0, "%s Exit!\n", indent);
		return ERROR;
	}

	Semaphore *thisSema = (Semaphore *) malloc(sizeof(Semaphore));
	if (thisSema == NULL) {
		TracePrintf(0, "%s Allocation Error!\n", indent);
		return ERROR;
	}
	memset(thisSema, 0, sizeof(Semaphore));
	thisSema->value = start_val;
	GetNewID(SEMA, &thisSema->object_node);
	*sem_idp = thisSema->object_node.id;
	TracePrintf(0, "%s Exit!\n", indent);
	return SUCCESS;
}

/* Increase number of available flags
 * 	After making a flag available
 *	if there is a process waiting for a flag
 *	put that flag in the ready queue.
 */
int KernelSemUp(int sem_id) {
	char *indent = "KernelSemUp:";
	TracePrintf(0, "%s Start!\n", indent);
	if (sem_id < 0) {
		TracePrintf(0, "%s ERROR: Invalid negative ID\n", indent);
		return ERROR;
	}
	//get semaphore pointer from id
	ObjectNode *object_node = Find(sem_id);
	if (NULL == object_node) {
		TracePrintf(0, "%s Error: no such semaphore!\n", indent);
		return ERROR;
	}
	Semaphore *thisSema = containerOf(object_node, Semaphore, object_node);
	TracePrintf(1, "%s Incrementing available flags\n", indent);
	thisSema->value++;
	//check if there is someone waiting for a flag
	if (FAILURE == QueueIsEmpty(&thisSema->down_queue)) {
		TracePrintf(1, "%s Give flag to head of queue\n", indent);
		Enqueue(ReadyQueue, Dequeue(&thisSema->down_queue));
	}
	TracePrintf(0, "%s Exit!\n", indent);
	return SUCCESS;
}

/* Decrease number of flags
 *	If there are no available flags
 *	wait in the 'down_queue' until somebody calls KernelSemUp
 *	Else, take a flag
 */
int KernelSemDown(int sem_id) {
	char *indent = "KernelSemDown:";
	TracePrintf(0, "%s Start!\n", indent);
	if (sem_id < 0) {
		TracePrintf(0, "%s ERROR: Invalid negative ID\n", indent);
		return ERROR;
	}
	//get semaphore pointer from id
	ObjectNode *object_node = Find(sem_id);
	if (NULL == object_node) {
		TracePrintf(0, "%s Error: no such semaphore!\n", indent);
		return ERROR;
	}
	Semaphore *thisSema = containerOf(object_node, Semaphore, object_node);
	while (thisSema->value == 0) {
		TracePrintf(1, "%s No available flags, waiting...\n", indent);
		Enqueue(&thisSema->down_queue, &KS->CurrentPCB->queue_node);
		if (FAILURE == KCSwitchToQueue(ReadyQueue)) {
			TracePrintf(0, "%s Failure, Bad context switch\n", indent);
			return ERROR;
		}
		if (NULL == Find(sem_id)) {
			TracePrintf(0, "%s Error: Semaphore reclaimed while waiting\n", indent);
			return ERROR;
		}
	}
	TracePrintf(1, "%s Decrementing available flags\n", indent);
	thisSema->value--;
	TracePrintf(0, "%s Exit!\n", indent);
	return SUCCESS;
}

/* Helper function:
 * 	If an IPC object is reclaimed with processes still waiting
 *	then these processes should cease waiting.
 *	When they are eventually switched to, there is an if clause
 *	that will return them with an error code */
void ReclaimIPCBlockQueue(Queue *Q) {
	Queue_Node *curNode = Q->head;
	while (NULL != curNode) {
		Enqueue(ReadyQueue, curNode);
		curNode = curNode->next;
	}
} 

/* Free a specific IPC object */
int KernelReclaim(int id) {
	char *indent = "KernelReclaim:";
	TracePrintf(0, "%s Start!\n", indent);
	if (id < 0) {
		TracePrintf(0, "%s ERROR: Invalid negative ID\n", indent);
		return ERROR;
	}
	ObjectNode *this_node = RemoveID(id);
	if (NULL == this_node) {
		TracePrintf(0, "This IPC has already been removed!\n");
		return ERROR;
	}
	switch (id % IPC_NUM){
		case PIPE:
			TracePrintf(3, "%s Reclaiming Pipe\n");
			Pipe *thisPipe = containerOf(this_node,
						     Pipe, object_node);
			ReclaimIPCBlockQueue(&thisPipe->read_queue);
			ReclaimIPCBlockQueue(&thisPipe->write_queue);
			free(thisPipe);
			break;
		case LOCK:
			TracePrintf(3, "%s Reclaiming Lock\n");
			Lock *thisLock = containerOf(this_node,
						     Lock, object_node);
			ReclaimIPCBlockQueue(&thisLock->acquire_queue);
			free(thisLock);
			break;
		case CVAR:
			TracePrintf(3, "%s Reclaiming Cvar\n");
			Cvar *thisCvar = containerOf(this_node, 
						     Cvar, object_node);
			ReclaimIPCBlockQueue(&thisCvar->wait_queue);
			free(thisCvar);
			break;
		case SEMA:
			TracePrintf(3, "%s Reclaiming Semaphore\n");
			Semaphore *thisSema = containerOf(this_node,
						 Semaphore, object_node);
			ReclaimIPCBlockQueue(&thisSema->down_queue);
			free(thisSema);
	}
	TracePrintf(0, "%s Exit!\n", indent);
}
