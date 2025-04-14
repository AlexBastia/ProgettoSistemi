#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

#include <uriscv/cpu.h>
#include <uriscv/liburiscv.h>
#include <uriscv/types.h>

#include "../../phase1/headers/asl.h"
#include "../../phase1/headers/pcb.h"

extern void uTLB_RefillHandler(void);

// exception handler gets the cause of the exception and calls the appropriate handler
void exceptionHandler(void);

static void syscallHandler(state_t*);
static void tlbExceptionHandler(state_t*);
void programTrapHandler(state_t*);
int findDeviceIndex(memaddr*);

#endif
