#include "./headers/pcb.h"

static struct list_head pcbFree_h;
static pcb_t pcbFree_table[MAXPROC];
static int next_pid = 1;

void initPcbs() {
  struct list_head* pcbst = &pcbFree_h;
  INIT_LIST_HEAD(pcbst);
  for (int i = 0; i < MAXPROC; i++) {
    freePcb(&pcbFree_table[i]);
  }
}

void freePcb(pcb_t* p) { list_add(&p->p_list, &pcbFree_h); }

pcb_t* allocPcb() {
  // Return NULL if pcbFree_h is emtpy
  if (list_empty(&pcbFree_h)) return NULL;

  // Remove from pcbFree_h
  struct list_head *removed = pcbFree_h.next;
  list_del(removed);
  //dato che *removed è un puntatore a list_head uso la macro container of per trovar il puntatore a pcb_t
  pcb_t *newPcb = container_of(removed, pcb_t, p_list);
  // Inizializza tutti i campi (default)
  INIT_LIST_HEAD(&newPcb->p_list);
  
  //notare che il campo padre è l'unico che è un semplice puntatore a pcb_t invece di una lista 
  newPcb->p_parent=NULL;

  INIT_LIST_HEAD(&newPcb->p_child);
  INIT_LIST_HEAD(&newPcb->p_sib);


  /*il campo p_s del processo è di tipo struct state_t, definito in types.h di uriscv 
  (vedete riga 10 del file types.h dentro la cartella headers)
  
  typedef struct state {
  unsigned int entry_hi;
  unsigned int cause;
  unsigned int status;
  unsigned int pc_epc;
  unsigned int mie;
  unsigned int gpr[STATE_GPR_LEN];
  } state_t;  

  ho pensato di fare una funzione privata che può servire se dovessimo avere bisogno altre volte di inizializzare
  una variabile di questo tipo, se doveste trovare macro/soluzioni migliori modificate pure come volete
  */
  newPcb->p_s = state_t_init(newPcb->p_s);

  newPcb->p_time=NULL;
  newPcb->p_semAdd=NULL;
  newPcb->p_supportStruct=NULL;
  newPcb->p_pid=next_pid;
  // Ritorna puntatore
  return newPcb;
}

static state_t state_t_init(state_t var){
  var.entry_hi=NULL;
  var.cause=NULL;
  var.status=NULL;
  var.pc_epc=NULL;
  var.mie=NULL;
  for(int i=0; i<STATE_GPR_LEN; i++){
    var.gpr[i]=NULL;
  }

  return var;
}

void mkEmptyProcQ(struct list_head* head) {}

int emptyProcQ(struct list_head* head) {}

void insertProcQ(struct list_head* head, pcb_t* p) {}

pcb_t* headProcQ(struct list_head* head) {}

pcb_t* removeProcQ(struct list_head* head) {}

pcb_t* outProcQ(struct list_head* head, pcb_t* p) {}

int emptyChild(pcb_t* p) {}

void insertChild(pcb_t* prnt, pcb_t* p) {}

pcb_t* removeChild(pcb_t* p) {}

pcb_t* outChild(pcb_t* p) {}
