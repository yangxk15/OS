#include "kernel_calls.h"

void TrapKernelHandler(UserContext *);
void TrapClockHandler(UserContext *);
void TrapIllegalHandler(UserContext *);
void TrapMemoryHandler(UserContext *);
void TrapMathHandler(UserContext *);
void TrapTtyReceiveHandler(UserContext *);
void TrapTtyTransmitHandler(UserContext *);
void TrapDiskHandler(UserContext *);
void TrapErrorHandler(UserContext *);

