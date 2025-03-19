#include "headers/exceptions.h"
#include "headers/interrupts.h"
#include <uriscv/types.h>
void uTLB_RefillHandler() {
    int prid = getPRID();
    setENTRYHI(0x80000000);
    setENTRYLO(0x00000000);
    TLBWR();
    LDST(GET_EXCEPTION_STATE_PTR(prid));
}

void exceptionHandler(){
    state_t *current_state = (GET_EXCEPTION_STATE_PTR(getPRID()));
    unsigned int cause = getCAUSE() & CAUSE_EXCCODE_MASK;  //as specified in phase 2 specs, use the bitwise AND to get the exception code

    if(CAUSE_IS_INT(cause)){
        interruptHandler(current_state, cause);  //if the cause is an interrupt, call the interrupt handler
    }else if(cause == EXC_ECU || cause == EXC_ECM){
        syscallHandler(current_state);
    }else if(cause >= EXC_MOD && cause <= EXC_UTLBS){
        tlbExceptionHandler(current_state);
    }else{
        programTrapHandler(current_state);
    }
}


void syscallHandler(state_t* state){
    int syscall_code = state->reg_a0;

    if(!(state->status & MSTATUS_MPP_MASK)){
        state->cause = PRIVINSTR;
        passUpordie(GENERALEXCEPT);
        return;
    }

    if(syscall_code>=1){
        passUpordie(GENERALEXCEPT);
        return;
    }
}
