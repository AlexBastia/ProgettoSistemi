#include "headers/exceptions.h"
#include "headers/interrupts.h"



void uTLB_RefillHandler() {
    int prid = getPRID();
    setENTRYHI(0x80000000);
    setENTRYLO(0x00000000);
    TLBWR();
    LDST(GET_EXCEPTION_STATE_PTR(prid));
}

void timer(cpu_t *time){
    STCK(*time);
}

void exceptionHandler(){
    cpu_t cpu_time_init, cpu_time_end;
    timer(&cpu_time_init); //call the timer function to update the time of the current process
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
    
    pcb_t *curr = current_process[getPRID()];
    timer(&cpu_time_end);  //call the timer function to update the time of the current process  
    curr->p_time += (cpu_time_end - cpu_time_init); //update the time of the current process

    //SE LE 3 RIGHE QUA SOPRA DANNO PROBLEMI RIVEDERE IMPLEMENTAZIONE
}

//this helper function is used to terminate the current process subtree 
static void terminateSubtree(pcb_t* process){  
    while(!emptyChild(process)){
        pcb_t* child = removeChild(process); //remove the first child of the process
        if(child!=NULL){
            terminateSubtree(child);
 //         outProcQ(&readyQueue, child);   aspetto implementazione readyqueue
            process_count--;
            freePcb(child);
        }
    }
}



static void syscallHandler(state_t* state){
    int syscall_code = state->reg_a0;

    if(!(state->status & MSTATUS_MPP_MASK) && syscall_code<0){ //if the MPP bit is not set, the syscall was called in user mode
        state->cause = PRIVINSTR; //set the cause to privileged instruction exception 
        passUpordie(GENERALEXCEPT); //call the passUpordie function to handle the exception as a Program trap
        return;
    }

    if(syscall_code>=1){ //if the syscall code is greater than 1, it is a syscall
        passUpordie(GENERALEXCEPT);
        return;
    }

    switch(syscall_code){
        case CREATEPROCESS:  //this call creates a new process
            ACQUIRE_LOCK(&global_lock); //acquire the lock to avoid race conditions
            pcb_t* newPCB = allocPcb(); //allocate a new PCB
            if(newPCB== NULL){  //if the PCB is NULL, the allocation failed for lack of resources
                RELEASE_LOCK(&global_lock);
                state->reg_a0 = -1;  //return -1 to signal the error
                break;
            }

            newPCB->p_s = *((state_t*)(state->reg_a1));  //copy the state given in a1 register to the new PCB 
            newPCB->p_supportStruct = (support_t*)(state->reg_a3); //set the support struct to the one given in a3 register

            pcb_t* parent = current_process[getPRID()]; //set the current process as the parent of the new process based on cpu number
            if(parent!=NULL){
                insertChild(parent, newPCB);
            }
            

            //insertProcQ(&readyQueue, newPCB);  aspetto implementazione readyQueue
            process_count++;
            RELEASE_LOCK(&global_lock);
            state->reg_a0 = newPCB->p_pid;  //return the pid of the new process
            break;

        case TERMPROCESS:
            ACQUIRE_LOCK(&global_lock); //this call terminates the current process
            int pid = state->reg_a1;  //get the pid of the process to be terminated
            pcb_t *tbt = NULL;  //initialize the pointer to the process to be terminated

            if(pid==0){
                tbt = current_process[getPRID()];  //if the pid is 0, terminate the current process
            }else{
                struct list_head* iter;
             /* list_for_each(iter, &readyQueue){
                    pcb_t* pcb = container_of(iter, pcb_t, p_list);   //aspetto readyQueue
                    if(pcb->p_pid == pid){
                        tbt = pcb;
                        break;
                    }
                }  */
            }

            if(tbt == NULL){
                    RELEASE_LOCK(&global_lock);
                    state->reg_a0 = -1;  //if the process to be terminated is not found, return -1
                    break;
            }

            if(!emptyChild(tbt)){
                  terminateSubtree(tbt);  //terminate the subtree of the process to be terminated
            }

            if(tbt->p_parent!=NULL){
                  outChild(tbt);  //remove the process from the parent's children list
            }

            //outProcQ(&readyQueue, tbt);  //remove the process from the ready queue
            process_count--;
            freePcb(tbt);  //free the PCB

            RELEASE_LOCK(&global_lock);
            scheduler();  //call the scheduler to select the next process
            break;
        case PASSEREN:
            ACQUIRE_LOCK(&global_lock);
            int *semAdd = state->reg_a1;  //get the semaphore address
            (*semAdd)--;  //decrement the semaphore value
            if(*semAdd < 0){  //if the semaphore address is negative, its value can't be decreased
               pcb_t* current = current_process[getPRID()];  //get the current process
               int ret = insertBlocked(semAdd, current);  //insert the current process in the blocked list of the semaphore

               state->pc_epc += 4;  //increment the program counter
               current->p_s = *state;  //save the state of the current process

               RELEASE_LOCK(&global_lock);
               scheduler();
               return;
            }
            RELEASE_LOCK(&global_lock);
            break;
        case VERHOGEN:
            ACQUIRE_LOCK(&global_lock);
            int *semAdd = state->reg_a1;  //get the semaphore address
            (*semAdd)++;  //increment the semaphore value
            if(*semAdd <= 0){  //if the semaphore value is less than or equal to 0, there are processes waiting on the semaphore
                pcb_t* unblocked = removeBlocked(semAdd);  //remove the first process from the blocked list of the semaphore
                if(unblocked!=NULL){
                    //insertProcQ(&readyQueue, unblocked);  aspetto readyQueue
                }
            }
            RELEASE_LOCK(&global_lock);
            break;
        case DOIO:
            ACQUIRE_LOCK(&global_lock);
            int *commandAddress = state->reg_a1;  //get the command address
            int commandValue = state->reg_a2;  //get the command value

            if(commandAddress==NULL){
                state->reg_a0 = -1;  //if the command address is NULL, return -1
                RELEASE_LOCK(&global_lock);
                break;
            }

            pcb_t *current = current_process[getPRID()];  //get the current process
            current-> p_s = *state;  //save the state of the current process

            //come lo trovo il semaforo associato al device?
            

        
    }

    state->pc_epc += 4;  //increment the program counter
    LDST(state);  //load the state
}
