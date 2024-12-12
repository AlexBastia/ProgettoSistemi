#include "./headers/asl.h"
#include "./headers/pcb.h"

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


int insertBlocked(int* semAdd, pcb_t* p) {
    // Insert the PCB pointed to by p at the tail of the process queue associated with the semaphore whose key is semAdd and set the semaphore address of p to semaphore with semAdd. If the semaphore is currently not active (i.e. there is no descriptor for it in the ASL), allocate a new descriptor from the semdFree list, insert it in the ASL (at the appropriate position), initialize all of the fields (i.e. set s_key to semAdd, and s_procq to mkEmptyProcQ()), and proceed as 4above. If a new semaphore descriptor needs to be allocated and the semdFree list is empty, return TRUE. In all other cases return FALSE. 

    semd_t * pos;

    // si scorre la lista semd_h per trovare il semaforo desiderato
    list_for_each_entry(pos, &semd_h, s_link){
        if(pos->s_key == semAdd){
            list_add_tail(&p->p_list, &pos->s_procq);
            return FALSE; //Probabilmente sbagliato :(
        }
    }

    // se sia semd_h non ha il semaforo con p e la lista semdFree_h è vuota si ritorna VERO
    if (list_empty(&semdFree_h)) return TRUE; 
    
    // se non si è trovato in semd_h si inizializza un semaforo libero in semdFree_h
    struct list_head* newSemList = list_next(&semdFree_h);
    list_del(newSemList);

    semd_t * newSem = container_of(newSemList, semd_t, s_link);
    // TODO inizializzazione del semaforo
    *newSem->s_key = *semAdd;

    // inizializzazione di sprocQ list head (no pointer) 
    INIT_LIST_HEAD(&newSem->s_procq);
    INIT_LIST_HEAD(&newSem->s_link);

    // inserimento del processo dentro il semaforo
    list_add_tail(&p->p_list, newSemList);

    // aggiunta del semaforo nella lista dei semafori :)
    list_add_tail(newSemList, &semd_h);

    return FALSE;
}

pcb_t* removeBlocked(int* semAdd) {
    semd_t * pos;
    list_for_each_entry(pos, &semd_h, s_link){
        if(pos->s_key == semAdd){
            pcb_t *p= removeProcQ(&pos->s_procq);
            if(emptyProcQ(&pos->s_procq)){
                list_del(&pos->s_link);
                list_add(&pos->s_link, &semdFree_h);
                return pos;
            };
        };
    };
    return NULL;
}
//supponiamo sia come outBlocked ma con in input solo il pid, che consegue dover scorrere tutti i semafori e tutte le loro queue
pcb_t* outBlockedPid(int pid) {
}

pcb_t* outBlocked(pcb_t* p) {
    semd_t* sem = container_of(p->p_semAdd, semd_t, s_key);
    return outProcQ(&sem->s_procq, p);
}

pcb_t* headBlocked(int* semAdd) {
}
