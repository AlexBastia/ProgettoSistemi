#include <uriscv/liburiscv.h>
#include "headers/initial.h"
#include "headers/scheduler.h"
#include "headers/interrupts.h"
#include "headers/exceptions.h"

#include "../phase1/headers/asl.h"
#include "../phase1/headers/pcb.h"

void Scheduler(){
    klog_print("Scheduler start");
    ACQUIRE_LOCK(&global_lock);
    klog_print("Scheduler lock acquired");
    if (emptyProcQ(&ready_queue)){ // TODO capire come implementare la readyQueue
        if (process_count == 0){
            RELEASE_LOCK(&global_lock);
            HALT();
        }
        else {
            // non ho ben capito, ma credo che il processo debba aspettare un interrupt

            //! capire cosa cuazzo è sta roba (l'ha scritta il prof, io non c'entro )
            unsigned int status = getSTATUS();
            status |= MSTATUS_MIE_MASK;
            setSTATUS(status);
            setMIE(MIE_ALL & ~MIE_MTIE_MASK);
            setTIMER(5000);
            WAIT(); // attesa di un interrupt

            // see Dott. Rovelli’s thesis for more details.
        }
    } else{
        pcb_t* next = removeProcQ(&ready_queue); // TODO: capire come implementare la readyQueue
        current_process[getPRID()] = next;
        setTIMER(TIMESLICE);
        RELEASE_LOCK(&global_lock);
        LDST(&next->p_s); // context switch al nuovo processo 

    }
    
}
