#ifndef	_page_table_h
#define	_page_table_h

#include "hardware.h"

#define INVALID 0xffffff

#define INTERFACE Page(KERNEL_STACK_BASE - 1)

#define SUCCESS 0
#define FAILURE -1
#define KILL -2 

#define USERTABLE 1
#define KERNELTABLE 0
#define NOTABLE -1

#define Page(i) ((u_int) i >> PAGESHIFT)
#define Address(i) ((u_int) i << PAGESHIFT)
#define FrameNumber(i) ((int *) Address(i))
#define PageType(i) ((i < MIN_VPN || i > MAX_VPN) ? NOTABLE : (i < MAX_PT_LEN ? KERNELTABLE : USERTABLE))
#define PageTable(i) (KERNELTABLE == PageType(i) ? KernelPageTable : KS->CurrentPCB->pagetable)	//use this only when PageType != NOTABLE

typedef struct pte PTE;

extern int VM_ENABLED;

extern PTE *KernelPageTable;

extern int getPTindexFromPN(int);
extern int PTEisValid(int);
extern int enablePTE(int, int, int);
extern int disablePTE(int, int);
extern int changePTE(int, int, int);

#endif
