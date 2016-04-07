#ifndef _data_structures_h
#define _data_structures_h

#include "process_control_block.h"

// Macro to get the position of a member in a larger struct
#define offsetOf(type, member) ((size_t) &((type *)0)->member)

// Macro to get the structure containing a queue_node
#define containerOf(ptr, type, member) ({ \
    const typeof( ((type *)0)->member ) *__mptr = (ptr); \
    (type *) ((char *)__mptr - offsetOf(type, member) ); })

typedef unsigned int u_int;

/* Kernel Stack
 * Is a union of both a pointer to the current PCB and a char array
 * Every process needs it's own Kernel Stack and it's own PCB,
 * so there is always a matching pair. 
 * Moreover, we need a convenient way of locating the current pcb 
 * at any given time, and the kernel stack is always in the same
 * location. This union performs that job well. 
 */
typedef union Kernel_Stack_Structure {
	PCB *CurrentPCB;
	char stack[KERNEL_STACK_MAXSIZE];
} KernelStack;

/* Terminal Receive Buffers 
 * For storing any input to any terminal
 * One queue of buffers per terminal
 * Queue structs are allocated in kernel_init
 */

typedef struct tty_buffer {
	char buf[TERMINAL_MAX_LINE];
	int len;
	int start;
	Queue_Node node;
} TtyBuffer;

extern KernelStack *KS;
extern u_int FreeFrameCount;

#endif
