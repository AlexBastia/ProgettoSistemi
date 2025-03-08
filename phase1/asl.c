#include "./headers/asl.h"
#include "./headers/pcb.h"

static semd_t semd_table[MAXPROC];
static struct list_head semdFree_h;
static struct list_head semd_h;

  /* 
    Initialize the semdFree list to contain all the elements 
    of the array static semd_t semdTable[MAXPROC]. 
    This method will be only called once during data structure initialization.
  */
  
void initASL() {
  // Inizializzazione liste semafori  
  INIT_LIST_HEAD(&semdFree_h);
  INIT_LIST_HEAD(&semd_h);
  
  // Spostamento di semafori da semd_tale alla lista di semafori liberi
  for (int i = 0; i < MAXPROC; i++) {
    list_add(&semd_table[i].s_link, &semdFree_h);
  }
}


/*
  Insert the PCB pointed to by p at the tail of the process queue associated with the semaphore
  whose key is semAdd and set the semaphore address of p to semaphore with semAdd. If the
  semaphore is currently not active (i.e. there is no descriptor for it in the ASL), allocate a new
  descriptor from the semdFree list, insert it in the ASL (at the appropriate position), initialize
  all of the fields (i.e. set s_key to semAdd, and s_procq to mkEmptyProcQ()), and proceed as 
  4above. If a new semaphore descriptor needs to be allocated and the semdFree list is empty,
  return TRUE. In all other cases return FALSE.
*/

int insertBlocked(int* semAdd, pcb_t* p) {

  semd_t* pos = NULL;

  // cerca il semaforo con questo semAdd e aggiunge il processo in coda alla processqueue
  list_for_each_entry(pos, &semd_h, s_link) {
    if (pos->s_key == semAdd) {
      list_add_tail(&p->p_list, &pos->s_procq);
      p->p_semAdd = semAdd;
      return FALSE;
    }
  }

  // se la semdFree è vuota si ritorna TRUE = 1
  if (list_empty(&semdFree_h)) {
    return TRUE;
  }

  // creazione di semd_t a partire da un elemento in semdFree_h
  semd_t* newSem = container_of(list_next(&semdFree_h), semd_t, s_link);
  newSem->s_key = semAdd;

  // creazione del processo vuoto e aggiunta del semaforo nella process queue con il semAdd dato come parametro
  mkEmptyProcQ(&newSem->s_procq);
  list_del(&newSem->s_link);
  list_add_tail(&newSem->s_link, &semd_h);
  list_add_tail(&p->p_list, &newSem->s_procq);
  p->p_semAdd = semAdd;

  return FALSE;
}  

/*
  Search the ASL for a descriptor of this semaphore. If none is found, return NULL; other-wise,
  remove the first (i.e. head) PCB from the process queue of the found semaphore de-
  scriptor and return a pointer to it. If the process queue for this semaphore becomes empty
  (emptyProcQ(s_procq) is TRUE), remove the semaphore descriptor from the ASL and return
  it to the semdFree list
*/

pcb_t* removeBlocked(int* semAdd) {
  semd_t* pos;
  list_for_each_entry(pos, &semd_h, s_link) {
    // Se trova il semaforo nella lista dei semafori attivi
    if (pos->s_key == semAdd) { 
      pcb_t* p = removeProcQ(&pos->s_procq);

      // se la coda dei processi del semaforo pos è vuota
      if (emptyProcQ(&pos->s_procq)) { 

        // si rimuove il semaforo dalla ASL e lo si inserisce nella coda dei semafori liberi  
        list_del(&pos->s_link);
        list_add(&pos->s_link, &semdFree_h);
      };
      return p; 
    };
  };
  return NULL;
}

// supponiamo sia come outBlocked ma con in input solo il pid, che consegue
// dover scorrere tutti i semafori e tutte le loro queue
/*pcb_t* outBlockedPid(int pid) {
}*/

pcb_t* outBlocked(pcb_t* p) {
  semd_t* pos = NULL;
  list_for_each_entry(pos, &semd_h, s_link) {
    if (pos->s_key == p->p_semAdd) {
      break;
    }
  }
  if (pos == NULL) return NULL;
  // prima facevamo con container of ma bisogna passargli una list_head* non il
  // semAdd che è un pcb_t*, ecco perche dava errore ora cerco il semaforo tra
  // quelli attivi, se lo trovo esco dal ciclo e faccio outProcq altrimenti
  // ritorno null
  pcb_t* p_out = outProcQ(&pos->s_procq, p);
  if (emptyProcQ(&pos->s_procq)) {
    list_del(&pos->s_link);
    list_add(&pos->s_link, &semdFree_h);
  }
  return p_out;
}

pcb_t* headBlocked(int* semAdd) { 
  semd_t* pos; // semaforo
  list_for_each_entry(pos, &semd_h, s_link) {  //scorro la lista dei semafori
    if (semAdd == pos->s_key) { //se il semaforo è quello cercato
      return headProcQ(&pos->s_procq); //ritorno il primo processo in coda
    }
  }
  return NULL;
}
