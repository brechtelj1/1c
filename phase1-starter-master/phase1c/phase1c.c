// James Brechtel and Zach


#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <usloss.h>
#include <phase1Int.h>

#define CHECKKERNEL() \
    if ((USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE) == 0) USLOSS_IllegalInstruction()

#define FREE 0
#define BUSY 1

typedef struct LockQ {
    int         pid;
    LockQ       *next;
} LockQ;

typedef struct Lock {
    int         inuse;
    char        name[P1_MAXNAME];
    int         state; // BUSY or FREE
    int         pid; // process id that currently holds lock
    int         vid; // condition variable for lock
    LockQ       ElQueue; // queue for processes waiting on lock
    // more fields here
} Lock;

static Lock locks[P1_MAXLOCKS];


// init locks. Must be called before other lock functions
void P1LockInit(void) {
    CHECKKERNEL();
    P1ProcInit();
    for (int i = 0; i < P1_MAXLOCKS; i++) {
        locks[i].inuse = FALSE;
    }
}

// create new lock named name. Return unique id for it in *lid.
// max of P1_MAXLOCKS. Id in range [0,P1_MAXLOCKS]
int P1_LockCreate(char *name, int *lid){
    int result = P1_SUCCESS;
    int i = 0;
    int lockId = -1;
    Lock currentLock;
    CHECKKERNEL();
    // disable interrupts
    P1DisableInterrupts();

    // check parameters
    for(i = 0; i < P1_MAXLOCKS;i++){
        if(strcmp(name,locks[i].name)){
            return P1_DUPLICATE_NAME;
        }
    }
    if(NULL == name){
        return P1_NAME_IS_NULL;
    }
    if(strlen(name) > P1_MAXNAME){
        return P1_NAME_TOO_LONG;
    }
    // check if all locks in use
    for(i = 0;i < P1_MAXLOCKS;i++){
        // open lock
        if(!locks[i].inuse){
            lockId = i;
            break;
        }
    }
    if(lockId == -1){
        return P1_TOO_MANY_LOCKS;
    }
    // find an unused Lock and initialize it
    currentLock = locks[lockId];

    strcpy(currntLock->name, name);
    currentLock.pid = P1_GetPid();
    currentLock.state = FREE;
    currentLock.inuse = 1;
    currentLock.ElQueue = malloc(sizeof(LockQ));
    currentLock.ElQueue->next = NULL;
    currentLock.ElQueue.pid = -1;
    currentLock.vid = -1;
    lid = lockId;
    
    // restore interrupts
    P1EnableInterrupts();
    return result;
}

int P1_LockFree(int lid) {
    int     result = P1_SUCCESS;
    Lock currentLock;
    CHECKKERNEL();
    // disable interrupts
    P1DisableInterrupts();

    // check if any processes are waiting on lock
    if(NULL == locks[lid].ElQueue->next){
        return P1_BLOCKED_PROCESSES; 
    }

    if(lid < 0 || lid >= P1_MAXLOCKS){
        return P1_INVALID_LOCK;
    }

    // mark lock as unused and clean up any state
    currentLock = locks[lid];
    strcpy(currentLock->name, "");
    currentLock.pid = -1;
    currentLock.state = FREE;
    currentLock.inuse = 0;
    currentLock.vid = -1;
    free(currentLock.ElQueue);

    // restore interrupts
    P1EnableInterrupts();
    return result;
}


int P1_Lock(int lid) {
    int result = P1_SUCCESS;
    LockQ curr;
    LockQ temp;
    Lock currentLock;

    CHECKKERNEL();

    if(lid < 0 || lid >= P1_MAXLOCKS){
        return P1_INVALID_LOCK;
    }

    currentLock = locks[lid];
    /*********************

    Pseudo-code from the lecture notes.

    while(1) {
          DisableInterrupts();
          if (lock->state == FREE) {
              lock->state = BUSY;
              break;
          }
          Mark process BLOCKED, add to lock->q
          RestoreInterrupts();
          Dispatcher();
    }
    RestoreInterrupts();

    *********************/
    while(1){
        P1DisableInterrupts();
        if(currentLock.state == FREE){
            currentLock.state = BUSY;
            break;
        }
        // gets current process id and sets to state blocked
        // vid is passed in as -1
        P1SetState(P1GetPid(), P1_STATE_BLOCKED, lid, currentLock.vid);
        
        // adds new process to head of locks queue
        curr = malloc(sizeof(LockQ));
        curr.pid = P1GetPid();
        curr->next = currentLock.ElQueue->next;
        currentLock.ElQueue->next = curr;
        // enable interrupts and dispatches
        P1EnableInterrupts();
        P1Dispatch(FALSE);
    }

    currentLock.pid = P1GetPid();

    P1EnableInterrupts();
    return result;
}


int P1_Unlock(int lid) {
    int result = P1_SUCCESS;
    LockQ curr;
    LockQ prev;
    Lock currentLock;

    CHECKKERNEL();

    if(lid < 0 || lid >= P1_MAXLOCKS){
        return P1_INVALID_LOCK;
    }
    currentLock = locks[lid];
    if(currentLock.pid != P1GetPid()){
        return P1_LOCK_NOT_HELD;
    }
    /*********************

      DisableInterrupts();
      lock->state = FREE;
      if (lock->q is not empty) {
          Remove process from lock->q, mark READY
          Dispatcher();
      }
      RestoreInterrupts();

    *********************/
    P1DisableInterrupts();
    currentLock.state = FREE;
    // if lock queue is not empty
    if(NULL != currentLock.ElQueue->next){
        curr = currentLock.ElQueue->next;
        prev = currentLock.ElQueue;
        // get to the tail of the queue
        while(NULL != curr->next){
            prev = curr;
            curr = curr->next;
        }
        // remove tail node
        prev->next = NULL;
        // set current process id to ready
        P1SetState(curr.pid, P1_STATE_READY, lid, currentLock.vid);
        // free previous tail node
        free(next);
        P1Dispatch(FALSE);
    }
    currentLock.pid = -1;
    P1EnableInterrupts();
    return result;
}

int P1_LockName(int lid, char *name, int len) {
    int result = P1_SUCCESS;
    Lock currentLock;
    int i;

    CHECKKERNEL();
    if(lid < 0 || lid >= P1_MAXLOCKS){
        return P1_INVALID_LOCK;
    }
    currentLock = locks[lid];
    if(NULL == currentLock->name){
        return P1_NAME_IS_NULL;
    }

    for(i = 0; i < len; i++){
        name[i] = currentLock->name[i];
        if(currentLock->name[i] == '\0'){
            break;
        }
    }
    return result;
}

/*
 * Condition variable functions.
 */

typedef struct Condition{
    int         inuse;
    char        name[P1_MAXNAME];
    int         lock;  // lock associated with this variable
    int         numWaiting;
    // more fields here
} Condition;

static Condition conditions[P1_MAXCONDS];

void P1CondInit(void) {
    CHECKKERNEL();
    P1LockInit();
    for (int i = 0; i < P1_MAXCONDS; i++) {
        conditions[i].inuse = FALSE;
    }
}

int P1_CondCreate(char *name, int lid, int *vid) {
    int result = P1_SUCCESS;
    int i;
    int condId = -1;
    CHECKKERNEL();
    // more code here
    P1DisableInterrupts();
    // error checks
    if(NULL == name){
        return P1_NAME_IS_NULL;
    }
    if(strlen(name) > P1_MAXNAME){
        return P1_NAME_TOO_LONG;
    }
    if(lid >= P1_MAXLOCKS || locks[lid].inuse == 0){
        return P1_INVALID_LOCK;
    }
    for(i = 0; i < P1_MAXCONDS; i++){
        if(strcmp(name, conditions[i]->name)){
            return P1_DUPLICATE_NAME;
        }
    }
    // find open condition
    for(i = 0; i < P1_MAXCONDS; i++){
        if(!conditions[i].inuse){
            condId = i;
            break;
        }
    }
    if(condId == -1){
        return P1_TOO_MANY_CONDS;
    }
    // set condition fields
    vid = condId;
    locks[lid].vid = condId;
    conditions[condId].lid = lid;
    conditions[condId].inuse = 1;
    strcpy(conditions[condId].name, name);
    conditions[CondId].numWaiting = 0;

    P1EnableInterrupts();
    return result;
}

int P1_CondFree(int vid) {
    int result = P1_SUCCESS;
    Condition currentCond;
    Lock currentLock;
    CHECKKERNEL();
    // more code here
    P1DisableInterrupts();

    // error checks
    if(conditions[vid].inuse){
        return P1_INVALID_COND;
    }
    currentCond = conditions[vid];
    currentLock = locks[currentCond.lid]
    if(NULL != currentLock.LockQ->next){
        return P1_BLOCKED_PROCESSES;
    }

    // reset condition feilds and locks condition variable
    strcpy(currentCond->name, "");
    currentCond.inuse = FALSE;
    currentCond.lid = -1;
    currentLock.vid = -1;

    P1EnableInterrupts();
    return result;
}


int P1_Wait(int vid) {
    int result = P1_SUCCESS;
    int checker;
    Condition currentCond;
    CHECKKERNEL();

    P1DisableInterrupts();

    if(vid > P1_MAXCONDS || conditions[vid].inuse == FALSE){
        return P1_INVALID_COND;
    }
    currentCond = conditions[vid];
    if(locks[currentCond.lid].inuse == FALSE){
        return P1_INVALID_LOCK;
    }
    if(P1GetPid() != locks[current.lid].pid){
        return P1_LOCK_NOT_HELD;
    }
    /*********************

      DisableInterrupts();
      Confirm lock is held
      cv->waiting++;
      Release(cv->lock);
      Make process BLOCKED, add to cv->q
      Dispatcher();
      Acquire(cv->lock);
      RestoreInterrupts();

    *********************/
    currentCond.numWaiting++;
    checker = P1_Unlock(currentCond.lid);
    // do error checks

    P1SetState(P1GetPid(), P1_STATE_BLOCKED, currentCond.lid, currentCond);
    // TODO: finish function
    P1EnableInterrupts();
    return result;
}

int P1_Signal(int vid) {
    int result = P1_SUCCESS;
        CHECKKERNEL();
    /*********************

      DisableInterrupts();
      Confirm lock is held 
      if (cv->waiting > 0) {
        Remove process from cv->q, make READY
        cv->waiting--;
        Dispatcher();
      }
      RestoreInterrupts();
    *********************/    
    return result;
}

int P1_Broadcast(int vid) {
    int result = P1_SUCCESS;
    CHECKKERNEL();
    /*********************
      DisableInterrupts();
      Confirm lock is held 
      while (cv->waiting > 0) {
        Remove process from cv->q, make READY
        cv->waiting--;
        Dispatcher();
      }
      RestoreInterrupts();
    *********************/    
    return result;
}

int P1_NakedSignal(int vid) {
    int result = P1_SUCCESS;
    CHECKKERNEL();
    // more code here
    return result;
}

int P1_CondName(int vid, char *name, int len) {
    int result = P1_SUCCESS;
    CHECKKERNEL();
    // more code here
    return result;
}
