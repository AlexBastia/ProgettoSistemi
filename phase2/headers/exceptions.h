#include <uriscv/liburiscv.h>
#include "initial.h"
#include <uriscv/cpu.h>
#include <uriscv/types.h>
#include "../../phase1/headers/pcb.h"
#include "../../phase1/headers/asl.h"


void uTLB_RefillHandler(void);

//exception handler gets the cause of the exception and calls the appropriate handler
void exceptionHandler(void);

void syscallHandler(state_t*);
void tlbExceptionHandler(state_t*);
void programTrapHandler(state_t*);