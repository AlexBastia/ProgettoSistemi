#include "./headers/asl.h"

static semd_t semd_table[MAXPROC];
static struct list_head semdFree_h;
static struct list_head semd_h;

void initASL() {
}

void addToPos(pcb_t* p, semd_t* sem){
    list_add_tail(&p->p_list, &sem->s_procq);    
}

int insertBlocked(int* semAdd, pcb_t* p) {
    //che cazzo sc
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
    semd_t * pos;
    list_for_each_entry(pos, &semd_h, s_link){
        if(pos->s_key == semAdd){
            pcb_t *p= removeProcQ(pos->s_procq);
            if(emptyProcQ(pos->s_procq)){
                list_del(&pos->s_link);
                list_add(&pos->s_link, &semdFree_h);
                return pos;
            };
        };
    };
    return NULL;
}


pcb_t* outBlockedPid(int pid) {

}

pcb_t* outBlocked(pcb_t* p) {

}

pcb_t* headBlocked(int* semAdd) {

}
