#include "yalnix.h"

char *indent = "===============";

int
main(void) {

  int pid, rc, status;


  pid = GetPid();

  TracePrintf(0, "%s I'm process %d, and I'm about to fork\n", indent, pid);
  
  while (-1 != (rc = Fork())) {
	Pause();
  }
  
  return;

  if (-1 == rc) return;

  if (0 == rc) {

    pid = GetPid();
    TracePrintf(0, "Hi, I'm the child is %d\n", pid);
    char *a[] = {"fork-demo", 0};
    Exec(a[0], a);
    Exit(21); // 
  }

  pid = rc;
  TracePrintf(0, "%s my child has pid %d\n", indent, pid);
  TracePrintf(0, "%s i'll wait for it to be done\n\n", indent);
  Pause();
  
  rc = Wait(&status);
  TracePrintf(0, "%s back from waitpid. rc = %d and status = %d\n", indent, rc, status);

  rc = Fork();
  if (0 == rc) {
    pid = GetPid();
    TracePrintf(0, "Second Round: Hi, I'm the child is %d\n", pid);
    Exit(pid); // 
  }


  pid = GetPid();
  return pid * 10;

}
