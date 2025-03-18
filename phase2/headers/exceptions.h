#include <uriscv/liburiscv.h>
#include "initial.h"
#include <uriscv/cpu.h>
void uTLB_RefillHandler(void);

//exception handler gets the cause of the exception and calls the appropriate handler
void exceptionHandler(void);

void syscallHandler(state_t*);
void interruptHandler();
void tlbExceptionHandler(state_t*);
void programTrapHandler(state_t*);