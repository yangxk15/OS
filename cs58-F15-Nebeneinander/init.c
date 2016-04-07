#include "yalnix.h"

int main(int argc, char* argv[]) {
	while (1) {
		TracePrintf(0, "********************************************************************************\n");
		TracePrintf(0, "***************************  Initial program is running!  **********************\n");
		TracePrintf(0, "********************************************************************************\n");
		TracePrintf(0, "GetPid: %d\n", GetPid());
//		int *intarray = (int *) malloc(sizeof(int) * 3000);
//		Delay(5);
		Pause();
	}
	return 0;
}
