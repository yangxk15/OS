#include "data_structures.h"

PTE *KernelPageTable = NULL;

/*  Get a new free frame from the "interface" page below the kernel stack
 *  The base argument is only used when the virtual memory is not enabled.
 *  Otherwise it is meaningless.
 *  NOTE: Use getFreeFrame _only_ if there is still frame
 */
int getFreeFrame(int base) {
	if (0 == VM_ENABLED) {
		FreeFrameCount--;
		return base;
	}
	int t = KernelPageTable[INTERFACE].pfn;
	KernelPageTable[INTERFACE].pfn = *FrameNumber(INTERFACE);
	WriteRegister(REG_TLB_FLUSH, Address(INTERFACE));
	FreeFrameCount--;
	TracePrintf(4, "getFrame:%d %d!\n", t, KernelPageTable[INTERFACE].pfn);
	return t;
}

/* setFrameFree reverses the process of getFreeFrame */
void setFrameFree(int pfn) {
	int t = KernelPageTable[INTERFACE].pfn;
	KernelPageTable[INTERFACE].pfn = pfn;
	WriteRegister(REG_TLB_FLUSH, Address(INTERFACE));
	*FrameNumber(INTERFACE) = t; 
	FreeFrameCount++;
}

/* Input: page number, output: appropriate page table index */
int getPTindexFromPN(int index) {
	int type = PageType(index);
	if (NOTABLE == type)
		return -1;
	if (KERNELTABLE == type)
		return index;
	return index - MAX_PT_LEN;
}

/* Returns whether specific page is valid */
int PTEisValid(int i) {
	int index = getPTindexFromPN(i);
	PTE *pageTable = PageTable(i);
	return pageTable[index].valid;
}

/* Enabling and disabling Page Table Entries is done in blocks
 * This way, we can decide in advance whether we have enough free
 * frames for a given job. Also, it avoids clunky loops.
 *
 * If a given PTE in the block is already enabled / disabled
 * these functions will skip over it and keep enabling the
 * surrounding PTEs. Such a situation may occur when setting
 * the brk or freeing an entire page table.
 */
int enablePTE(int start, int end, int protection) {

	

	int start_index = getPTindexFromPN(start);
	int   end_index = getPTindexFromPN(end - 1) + 1;

	if (-1 == start_index || PageType(start) != PageType(end - 1)) {
		TracePrintf(4, "Enabling page table entry falied! \
				The index is illegal!\n");
		return FAILURE;
	}

	PTE *pageTable = PageTable(start);

	u_int i = 0;
	int pages_needed = 0;
	for (i = start_index; i < end_index; i++) {
		if (0 == pageTable[i].valid) {
			pages_needed++;
		}
	}

	if (pages_needed > FreeFrameCount) {
		TracePrintf(4, "Enabling page table entry falied! \
				Not enough free frames!\n");
		return FAILURE;
	}

	for (i = start_index; i < end_index; i++) {
		if (1 == pageTable[i].valid) {
			continue;
		}
		pageTable[i].valid = 1;
		pageTable[i].prot = protection;
		pageTable[i].pfn = getFreeFrame(i);
	}

	if (0 != VM_ENABLED) {
		if (pageTable == KernelPageTable) {
			WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);
		} else {
			WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);
		}
	}

	TracePrintf(4, "Page table entries %d to %d enabled!\n", start, end);
	return SUCCESS;
}

int disablePTE(int start, int end) {

	int start_index = getPTindexFromPN(start);
	int   end_index = getPTindexFromPN(end - 1) + 1;

	if (-1 == start_index || PageType(start) != PageType(end - 1)) {
		TracePrintf(4, "Enabling page table entry falied! \
				The index is illegal!\n");
		return FAILURE;
	}

	PTE *pageTable = PageTable(start);

	u_int i = 0;
	for (i = start_index; i < end_index; i++) {
		if (0 == pageTable[i].valid) {
			continue;
		}
		setFrameFree(pageTable[i].pfn);
		pageTable[i].valid = 0;
		pageTable[i].prot = PROT_NONE;
		pageTable[i].pfn = INVALID;
	}

	if (pageTable == KernelPageTable) {
		WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);
	} else {
		WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);
	}

	TracePrintf(4, "Disabling page table entry %d to %d succeeded!\n",
			start, end);
	return SUCCESS;
}

/* This function changes a specific PTE
 * It is useful when changing the KernelStack from one process to another
 * because we don't want to free the associate frames, and we don't want
 * to disable the PTEs. 
 */
int changePTE(int i, int protection, int pfn) {

	int index = getPTindexFromPN(i);

	if (-1 == index) {
		TracePrintf(4, "Changing page table entry protection falied! \
				The index %d is illegal!\n", i);
		return FAILURE;
	}

	PTE *pageTable = PageTable(i);

	if (0 == pageTable[index].valid) {
		TracePrintf(4, "Changing page table entry protection falied! \
				The entry %d has not been enabled!\n", i);
		return FAILURE;
	}

	int old_pfn = pageTable[index].pfn;
	if (-1 != protection)
		pageTable[index].prot = protection;

	if (-1 != pfn)
		pageTable[index].pfn = pfn;

	if (0 != VM_ENABLED)
		WriteRegister(REG_TLB_FLUSH, Address(i));

	TracePrintf(4, "Changing page table entry %d succeeded! \
			 Now the protection is %d, and the pfn is %d.\n",
			 i, pageTable[index].prot, pageTable[index].pfn);
	return old_pfn;
}

