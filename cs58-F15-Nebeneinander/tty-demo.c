#include "yalnix.h"

char *indent = "===============";

int
main(void) {
	int pid;
	int *status = (int *) malloc(sizeof(int));
	pid = Fork();
	if (0 == pid) {
		char d[3000] = {0};
		TtyRead(0, d, 10);
		TtyPrintf(1, d);
		pid = Fork();
		if (0 == pid) {
			char e[3000] = {0};
			TtyRead(0, d, 1000);
			TtyPrintf(1, d);
			Exit(0);
		}
		Wait(status);
		Exit(0);
	}
	char c[3000] = {0};
	TtyRead(0, c, 10);
	TtyPrintf(1, c);
	Wait(status);
	return 0;
}
