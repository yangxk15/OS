#include "yalnix.h"

#define RPIPE 0  // the pipe we read from
#define WPIPE 1  // the pipe we write to

char *indent = "===============";

int main() {
  int pipe1;
  int pid, other_pid;
  int bytes;

  if (PipeInit(&pipe1) < 0) {
    TtyPrintf(0, "PipeInit error!\n");
    exit(-1);
  }


  pid = Fork();

  if (pid < 0) {
    TtyPrintf(0, "Fork error!\n");
    Exit(-1);
  }

  if (0 == pid) {
    //----------------------child-------------------

    pid = GetPid(); 

    while (1) {
      int i;
      int length = rand() % 20;
      char *a = (char *) malloc(sizeof(char) * (length + 1));
      for (i = 0; i < length; i++) {
	a[i] = 'a' + rand() % 26;
      }
      a[i] = 0;
      // write my pid to the parent
      TtyPrintf(0, "%s Write to parent %d chars: %s\n", indent, length, a); 
      bytes = PipeWrite(pipe1, a, length);
      TtyPrintf(0, "%s Write %d chars finished\n", indent, bytes); 
      TtyPrintf(0, "%s Another round!\n", indent);
      free(a);
      Pause();
    }
  }

  //----------------------parent-------------------


  // get the pid of child
  while (1) {
    int i;
    int length = rand() % 15;
    char b[100] = {0};
    TtyPrintf(0, "read from child %d chars\n", length); 
    bytes = PipeRead(pipe1, b, length);
    TtyPrintf(0, "read %d chars: %s\n", bytes, b); 
    TtyPrintf(0, "Another round!\n", indent);
    Pause();
  }

}  




