#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H


#include <uriscv/liburiscv.h>
#include "initial.h"
#include <uriscv/cpu.h>
#include <uriscv/types.h>
#include "../../phase1/headers/pcb.h"
#include "../../phase1/headers/asl.h"

extern void uTLB_RefillHandler(void);

//exception handler gets the cause of the exception and calls the appropriate handler
void exceptionHandler(void);

static void syscallHandler(state_t*);
static void tlbExceptionHandler(state_t*);
void programTrapHandler(state_t*);

#endif