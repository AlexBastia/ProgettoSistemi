#include "./headers/asl.h"

static semd_t semd_table[MAXPROC];
static struct list_head semdFree_h;
static struct list_head semd_h;

void initASL() { 
    // Initialize the semdFree list to contain all the elements of the array static semd_t semdTable[MAXPROC]. This method will be only called once during data structure initialization.


    // sentinella 
    INIT_LIST_HEAD(&semdFree_h);

    for (int i = 0; i < MAXPROC; i++){
        list_add(&semd_table[i], &semdFree_h);
    }

}

void addToPos(pcb_t* p, semd_t* sem){
    list_add_tail(&p->p_list, &sem->s_procq);
}

int insertBlocked(int* semAdd, pcb_t* p) {
    // Insert the PCB pointed to by p at the tail of the process queue associated with the semaphore whose key is semAdd and set the semaphore address of p to semaphore with semAdd. If the semaphore is currently not active (i.e. there is no descriptor for it in the ASL), allocate a new descriptor from the semdFree list, insert it in the ASL (at the appropriate position), initialize all of the fields (i.e. set s_key to semAdd, and s_procq to mkEmptyProcQ()), and proceed as 4above. If a new semaphore descriptor needs to be allocated and the semdFree list is empty, return TRUE. In all other cases return FALSE. 


    semd_t * pos;

    list_for_each_entry(pos, &semd_h, s_link){
        if(pos->s_key == semAdd){
            addToPos(p, pos);
            return FALSE; //Probabilmente sbagliato :(
        }
    }
    if (list_empty(&semdFree_h)) return TRUE; //Cazzo
    
    struct list_head* newSemList = list_next(&semdFree_h);
    list_del(newSemList);

    semd_t * newSem = container_of(newSemList, semd_t, s_link);
    // inizializzazione del semaforo
    *newSem->s_key = *semAdd;
    // assumiamo che procQ sia gia inizializzato dalla funzione 
    // initASL(), altrimenti e' inutile
    addToPos(p, newSem);
    list_add_tail(newSemList, &newSem->s_procq);

    return FALSE;
}

pcb_t* removeBlocked(int* semAdd) {
    
}

pcb_t* outBlockedPid(int pid) {

}

pcb_t* outBlocked(pcb_t* p) {

}

pcb_t* headBlocked(int* semAdd) {

}
