#include "./headers/asl.h"
#include "./headers/pcb.h"

static semd_t semd_table[MAXPROC];
static struct list_head semdFree_h;
static struct list_head semd_h;

void initASL() { 
    // Initialize the semdFree list to contain all the elements of the array static semd_t semdTable[MAXPROC]. This method will be only called once during data structure initialization.

    // sentinella 
    INIT_LIST_HEAD(&semdFree_h);
    INIT_LIST_HEAD(&semd_h);
    for (int i = 0; i < MAXPROC; i++){
        list_add(&semd_table[i].s_link, &semdFree_h);
    }

} //dava kernel panic perché non venivano inizializzati semd_h e su list_add stavamo passando dei dati 
// di tipo semd invece che delle list_head*


int insertBlocked(int* semAdd, pcb_t* p) {
    semd_t *pos=NULL;
    //cerca il semaforo con questo semAdd e aggiunge il processo in coda alla processqueue
    list_for_each_entry(pos, &semd_h, s_link){
        if(pos->s_key ==semAdd){
            list_add_tail(&p->p_list, &pos->s_procq);
            p->p_semAdd=semAdd;
            return FALSE;
        }
    }

    //se non lo trova bisogna prenderne uno da semdfree_h, sempre che questa non sia vuota
    if(list_empty(&semdFree_h)){
        return TRUE;
    }

    semd_t *newSem= container_of(list_next(&semdFree_h), semd_t, s_link);
    newSem->s_key=semAdd;
    mkEmptyProcQ(&newSem->s_procq);
    list_del(&newSem->s_link);
    list_add_tail(&newSem->s_link, &semd_h);
    list_add_tail(&p->p_list, &newSem->s_procq);
    p->p_semAdd=semAdd;
    return FALSE;
}  //questa l'ho riscritta da capo per evitare problemi

pcb_t* removeBlocked(int* semAdd) {
    semd_t * pos;
    list_for_each_entry(pos, &semd_h, s_link){
        if(pos->s_key == semAdd){
            pcb_t *p= removeProcQ(&pos->s_procq);
            if(emptyProcQ(&pos->s_procq)){
                list_del(&pos->s_link);
                list_add(&pos->s_link, &semdFree_h);
            };
            return p;  //qui ho fixato non mi ricordo come ma era una cagata
        };
    };
    return NULL;
}
//supponiamo sia come outBlocked ma con in input solo il pid, che consegue dover scorrere tutti i semafori e tutte le loro queue
/*pcb_t* outBlockedPid(int pid) {
}*/

pcb_t* outBlocked(pcb_t* p) {
    semd_t * pos=NULL;
    list_for_each_entry(pos, &semd_h, s_link){
        if(pos->s_key == p->p_semAdd){
            break;
        }
    }
    if (pos == NULL) return NULL;
    //prima facevamo con container of ma bisogna passargli una list_head* non il semAdd che è un pcb_t*, ecco perche dava errore
    //ora cerco il semaforo tra quelli attivi, se lo trovo esco dal ciclo e faccio outProcq altrimenti ritorno null
    return outProcQ(&pos->s_procq, p);
}

pcb_t* headBlocked(int* semAdd) {
    semd_t * pos;
    list_for_each_entry(pos, &semd_h, s_link){
        if(semAdd == pos->s_key){
            return headProcQ(&pos->s_procq);
        }
    }
    return NULL;
}
