#include <uriscv/liburiscv.h>
#include "headers/initial.h"

#include "../phase1/headers/asl.h"
#include "../phase1/headers/pcb.h"

void Scheduler(){
    ACQUIRE_LOCK(&global_lock);

    // Your Nucleus should guarantee finite progress; consequently, every ready process will have an opportuity to execute. The Nucleus should implement a simple preemptive round-robin scheduling algorithm with a time slice value of 5 milliseconds (constant TIMESLICE). Preemptive CPU scheduling requires the use of an interrupt generating system clock. µRISC-V offers two choices: the single system-wide. Interval Timer or a Processor’s Local Timer (PLT). One should use the PLT to support per processor scheduling since the Interval Timer is reserved for implementing Pseudo-clock ticks [Section 7.3]. In its simplest form whenever the Scheduler is called it should dispatch the “next” process in the Ready Queue

    // In  its simplest form whenever the Scheduler is called it should dispatch the “next” process in the Ready Queue.
    // 1. Remove the PCB from the head of the Ready Queue and store the pointer to the PCB in the Current Process field of the current CPU.
    // 2. Load 5 milliseconds on the PLT [Section 7.2].
    // 3. Perform a Load Processor State (LDST) on the processor state stored in PCB of the Current Process (p_s) of the current CPU.

//     Dispatching a process transitions it from a “ready” process to a “running” process and set the IRT
    // to 0.
    // The Scheduler should behave in the following manner if the Ready Queue is empty:
    // 1. If the Process Count is 0, invoke the HALT BIOS service/instruction. Consider this a job well
    // done!
    // 2. If the Process Count > 0 enter a Wait State. A Wait State is where the processor is not executing
    // instructions, but “twiddling its thumbs” waiting for a device interrupt to occur. µRISC-V
    // supports a WAIT instruction expressly for this purpose. Before executing the WAIT instruction,
    // the Scheduler must first set TPR register to 1.
    // Important: Before executing the WAIT instruction, the Scheduler must first set the mie register
    // to enable interrupts and either disable the PLT (also through the mie register) using:
    // setMIE(MIE_ALL & ~MIE_MTIE_MASK);
    // unsigned int status = getSTATUS();
    // status |= MSTATUS_MIE_MASK;
    // setSTATUS(status);
    // see Dott. Rovelli’s thesis for more details.
    // The first interrupt that occurs after entering a Wait State should not be for the PLT.
    // Important: The ready queue is a shared data structure between the CPUs. So before access or
    // change it, you must acquire the Global Lock with ACQUIRE_LOCK. Remember to release the Global
    // Lock with RELEASE_LOCK when it is no more necessary.



    if (emptyProcQ(&readyQueue)){ // TODO capire come implementare la readyQueue
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

        }
    } else{
        pcb_t* next = removeProcQ(&readyQueue); // TODO: capire come implementare la readyQueue
        current_process[getPRID()] = next;
        setTIMER(TIMESLICE);
        LDST(&next->p_s); // context switch al nuovo processo 
    }
    RELEASE_LOCK(&global_lock);
}
