#include "headers/exceptions.h"

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
        interruptHandler();
    }else if(cause == EXC_ECU || cause == EXC_ECM){
        syscallHandler(current_state);
    }else if(cause >= EXC_MOD && cause <= EXC_UTLBS){
        tlbExceptionHandler(current_state);
    }else{
        programTrapHandler(current_state);
    }
}