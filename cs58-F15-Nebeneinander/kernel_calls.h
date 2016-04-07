#include "yalnix.h"
#include "inter_process_com.h"

extern int KernelFork();
extern int KernelExec(char *, char **);
extern void KernelExit(int);
extern int KernelWait(int *); 
extern int KernelGetPid();
extern int KernelUserBrk(void *);
extern int KernelDelay(int);
extern void DecrementDelayQueue();
extern int KernelTtyRead(int, void *, int);
extern int KernelTtyWrite(int, void *, int);
extern int KernelPipeInit(int *);
extern int KernelPipeRead(int, void *, int);
extern int KernelPipeWrite(int, void *, int);
extern int KernelLockInit(int *);
extern int KernelAcquire(int);
extern int KernelRelease(int);
extern int KernelCvarInit(int *);
extern int KernelCvarSignal(int);
extern int KernelCvarBroadcast(int);
extern int cvarDequeue(int cvar_id);
extern int KernelCvarWait(int, int);
extern int KernelSemInit(int *, int);
extern int KernelSemUp(int);
extern int KernelSemDown(int);
extern int KernelReclaim(int);
