// James Brechtel and Zachery Braaten-Schuettpelz

#include <stdio.h>
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

// struct that creates nodes for the Queue of objects
// in line to grab the lock
typedef struct LockQ {
    int             pid;
    struct LockQ    *next;
} LockQ;

// struct that creates lock "object" and its associated variables
typedef struct Lock {
    int         inuse;
    char        name[P1_MAXNAME];
    int         state;              // BUSY or FREE
    int         pid;                // process id that currently holds lock
    int         vid;                // condition variable for lock
    LockQ       ElQueue;            // queue for processes waiting on lock
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
    int interruptVal;
    int result = P1_SUCCESS;
    int i = 0;
    int lockId = -1;
    Lock *currentLock;
    LockQ *elQueue; 
    CHECKKERNEL();
    // disable interrupts
    interruptVal = P1DisableInterrupts();

    if(NULL == name){
        return P1_NAME_IS_NULL;
    }
    // check parameters
    for(i = 0; i < P1_MAXLOCKS;i++){
        if(strcmp(name,locks[i].name) == 0){
            return P1_DUPLICATE_NAME;
        }
    }
    if(strlen(name) > P1_MAXNAME){
        return P1_NAME_TOO_LONG;
    }
    // check if all locks in use
    for(i = 0;i < P1_MAXLOCKS;i++){
        // open lock
        if(locks[i].inuse == 0){
            lockId = i;
            break;
        }
    }
    if(lockId == -1){
        return P1_TOO_MANY_LOCKS;
    }

    // find an unused Lock and initialize it
    currentLock = &locks[lockId];
    elQueue = malloc(sizeof(LockQ));

    strcpy(currentLock->name, name);
    currentLock->pid = P1_GetPid();
    currentLock->state = FREE;
    currentLock->inuse = 1;
    currentLock->ElQueue = *elQueue;
    currentLock->ElQueue.next = NULL;
    currentLock->ElQueue.pid = -1;
    currentLock->vid = -1;
    *lid = lockId;
    
    // restore interrupts
    P1EnableInterrupts();
    return result;
}

// Checks to see if the lock trying to be freed has processes blocked
// If there are no blocked processes then set all fields of the struct
// to their original values
int P1_LockFree(int lid) {
    int     result = P1_SUCCESS;
    Lock *currentLock;
    CHECKKERNEL();
    // disable interrupts
    int interruptVal = P1DisableInterrupts();
    if(interruptVal);
    // check if any processes are waiting on lock
    if(NULL == locks[lid].ElQueue.next){
        return P1_BLOCKED_PROCESSES; 
    }

    if(lid < 0 || lid >= P1_MAXLOCKS){
        return P1_INVALID_LOCK;
    }

    // mark lock as unused and clean up any state
    currentLock = &locks[lid];
    strcpy(currentLock->name, "");
    currentLock->pid = -1;
    currentLock->state = FREE;
    currentLock->inuse = 0;
    currentLock->vid = -1;

    // restore interrupts
    P1EnableInterrupts();
    return result;
}

// Gives the current process the lock specified if no other process is holding
// the current lock. If another process is holding the specified lock add that
// process to a queue of processes that are waiting to acquire the lock
int P1_Lock(int lid) {
    int interruptVal;
    int stateVal;
    int result = P1_SUCCESS;
    //int i = 0;
    LockQ *curr;
    //LockQ temp;
    Lock *currentLock;

    CHECKKERNEL();
    if(lid < 0 || lid >= P1_MAXLOCKS || locks[lid].inuse == 0){
        return P1_INVALID_LOCK;
    }

    currentLock = &locks[lid];
    while(1){
        interruptVal = P1DisableInterrupts();
        if(currentLock->state == FREE){
            //printf("lock status now busy\n");
            currentLock->state = BUSY;
            break;
        }
        // gets current process id and sets to state blocked
        // vid is passed in as -1
        stateVal = P1SetState(P1_GetPid(), P1_STATE_BLOCKED, lid, currentLock->vid);
        
        // adds new process to head of locks queue
        curr = malloc(sizeof(LockQ));
        curr->pid = P1_GetPid();
        curr->next = currentLock->ElQueue.next;
        currentLock->ElQueue.next = curr;
        // enable interrupts and dispatches
        P1EnableInterrupts();
        P1Dispatch(FALSE);
    }
    currentLock->inuse = 1;
    currentLock->pid = P1_GetPid();

    P1EnableInterrupts();
    return result;
}

// Releases the currently held lock by the process. If There is a process
// in the queue waiting for the lock, set that process's state to ready
// If there is no other processes in the queue then set the current pid to -1
int P1_Unlock(int lid) {
    int result = P1_SUCCESS;
    LockQ *curr;
    LockQ *prev;
    Lock *currentLock;
    int interruptVal;
    int stateVal;

    CHECKKERNEL();

    if(lid < 0 || lid >= P1_MAXLOCKS || locks[lid].inuse == 0){
        //printf("invalid lock P1_unlock\n");
        return P1_INVALID_LOCK;
    }
    currentLock = &locks[lid];
    if(currentLock->pid != P1_GetPid()){
        return P1_LOCK_NOT_HELD;
    }
    interruptVal = P1DisableInterrupts();
    if(interruptVal);

    currentLock->state = FREE;
    // if lock queue is not empty
    if(NULL != currentLock->ElQueue.next){
        curr = currentLock->ElQueue.next;
        prev = &currentLock->ElQueue;
        // get to the tail of the queue
        while(NULL != curr->next){
            prev = curr;
            curr = curr->next;
        }
        // remove tail node
        prev->next = NULL;
        // set current process id to ready
        stateVal = P1SetState(curr->pid, P1_STATE_READY, lid, currentLock->vid);
        // free previous tail node
        //free(prev);
        P1Dispatch(FALSE);
    }
    currentLock->pid = -1;
    P1EnableInterrupts();
    return result;
}

// This function copies len characters from the specified lock
// into name
int P1_LockName(int lid, char *name, int len) {
    int result = P1_SUCCESS;
    Lock *currentLock;
    int i;

    CHECKKERNEL();
    if(lid < 0 || lid >= P1_MAXLOCKS || locks[lid].inuse == 0){
        return P1_INVALID_LOCK;
    }
    currentLock = &locks[lid];
    
    if(NULL == name){
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
    int         lid;                // lock associated with this variable
    int         numWaiting;
    LockQ       CondQueue;
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

// creates new condition variable for lock lid named name and returns a unique id
// for it in *vid. assume max P1_MAXCONDS condition variables, id must be in rage 0-MAXCOND
int P1_CondCreate(char *name, int lid, int *vid) {
    int result = P1_SUCCESS;
    int i;
    int condId = -1;
    LockQ *CondQueue; 
    CHECKKERNEL();
    
    // more code here
    int interruptVal = P1DisableInterrupts();
    if(interruptVal);
    // error checks
    if(NULL == name){
        P1EnableInterrupts();
        return P1_NAME_IS_NULL;
    }
    if(strlen(name) > P1_MAXNAME){
        P1EnableInterrupts();
        return P1_NAME_TOO_LONG;
    }
    // NOTE original -> locks[lid].inuse == 0
    if(lid >= P1_MAXLOCKS || lid < 0){
        P1EnableInterrupts();
        //printf("invalid lock P1cond create\n");
        return P1_INVALID_LOCK;
    }

    for(i = 0; i < P1_MAXCONDS; i++){
        if(strcmp(name, conditions[i].name) == 0){
            P1EnableInterrupts();
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
    vid = &condId;
    locks[lid].vid = condId;
    conditions[condId].lid = lid;
    conditions[condId].inuse = 1;
    strcpy(conditions[condId].name, name);
    conditions[condId].numWaiting = 0;
    CondQueue = malloc(sizeof(LockQ));
    conditions[condId].CondQueue = *CondQueue;
    conditions[condId].CondQueue.next = NULL;
    conditions[condId].CondQueue.pid = -1;

    P1EnableInterrupts();
    return result;
}

// This function frees the condition variable.
// This throws an error if there are conditions waiting for the lock
// otherwhise resets the fields of the condition variable
int P1_CondFree(int vid) {
    int result = P1_SUCCESS;
    Condition *currentCond;
    Lock *currentLock;
    CHECKKERNEL();
    // more code here
    int interruptVal = P1DisableInterrupts();
    if(interruptVal);
    // error checks
    if(conditions[vid].inuse){
        P1EnableInterrupts();
        return P1_INVALID_COND;
    }
    currentCond = &conditions[vid];
    currentLock = &locks[currentCond->lid];
    if(NULL != currentCond->CondQueue.next){
        P1EnableInterrupts();
        return P1_BLOCKED_PROCESSES;
    }

    // reset condition feilds and locks condition variable
    strcpy(currentCond->name, "");
    currentCond->inuse = FALSE;
    currentCond->lid = -1;
    currentLock->vid = -1;

    P1EnableInterrupts();
    return result;
}

// Waits on the condition variable. The current process must hold
// the lock and will be released while waiting. While the process is
// waiting its state is set to blocked
int P1_Wait(int vid) {
    int result = P1_SUCCESS;
    int checker;
    Condition *currentCond;
    LockQ *newNode;
    CHECKKERNEL();
    int stateVal;
    int lockVal;

    int interruptVal = P1DisableInterrupts();
    if(interruptVal);

    if(vid > P1_MAXCONDS || conditions[vid].inuse == FALSE){
        P1EnableInterrupts();
        return P1_INVALID_COND;
    }
    currentCond = &conditions[vid];

    // lock id bad   (CHANGED FROM FALSE TO TRUE (this lock right?))
    if(currentCond->lid > P1_MAXLOCKS){
        P1EnableInterrupts();
        return P1_INVALID_LOCK;
    }
    if(locks[currentCond->lid].state == FREE){
        P1EnableInterrupts();
        return P1_LOCK_NOT_HELD;
    }

    currentCond->numWaiting++;
    checker = P1_Unlock(currentCond->lid);
    // do error checks

    stateVal = P1SetState(P1_GetPid(), P1_STATE_BLOCKED, currentCond->lid, vid);
    if(stateVal);

    // adds process to queue
    newNode = malloc(sizeof(LockQ));
    newNode->pid = P1_GetPid();

    // wedge new node between empty head node and next node
    newNode->next = currentCond->CondQueue.next;
    currentCond->CondQueue.next = newNode;

    P1Dispatch(FALSE);
    lockVal = P1_Lock(currentCond->lid);
    if(lockVal);
    P1EnableInterrupts();
    return result;
}

// This function signals a process that is waiting on the condition
// variable. If there are no process waiting on the condition variable,
// P1_Signal does nothing.
int P1_Signal(int vid) {
    int result = P1_SUCCESS;
    Condition *currentCond;
    LockQ *curr;
    LockQ *prev;
    int stateVal;
    int interruptVal;
    CHECKKERNEL();
    interruptVal = P1DisableInterrupts();
    if(interruptVal);
    if(vid > P1_MAXCONDS || conditions[vid].inuse == FALSE){
        P1EnableInterrupts();
        return P1_INVALID_COND;
    }
    currentCond = &conditions[vid];
    if(locks[currentCond->lid].inuse == FALSE){
        P1EnableInterrupts();
        return P1_INVALID_LOCK;
    }
    if(P1_GetPid() != locks[currentCond->lid].pid){
        P1EnableInterrupts();
        return P1_LOCK_NOT_HELD;
    }

    curr = &currentCond->CondQueue;  
    while(NULL != curr->next){
            printf("elQueue = %d\n",curr->pid);
            curr = curr->next;
    }
    if(currentCond->numWaiting > 0){
        curr = currentCond->CondQueue.next;
        prev = &currentCond->CondQueue;
        while(NULL != curr->next){
            prev = curr;
            curr = curr->next;
        }
        prev->next = NULL;
        stateVal = P1SetState(curr->pid, P1_STATE_READY, currentCond->lid, vid);
        if(stateVal);
        currentCond->numWaiting--;
        P1Dispatch(FALSE);
    }
    P1EnableInterrupts();
    return result;
}

// This function signals all process that are waiting on the
// condition variable. If there are no process waiting on the
// condition variable, this function does nothing.
int P1_Broadcast(int vid) {
    int result = P1_SUCCESS;
    Condition *currentCond;
    LockQ *curr;
    LockQ *head;
    int stateVal;
    int interruptVal;
    CHECKKERNEL();
    interruptVal = P1DisableInterrupts();
    if(interruptVal);
    if(vid > P1_MAXCONDS || conditions[vid].inuse == FALSE){
        P1EnableInterrupts();
        return P1_INVALID_COND;
    }
    currentCond = &conditions[vid];
    if(locks[currentCond->lid].inuse == FALSE){
        P1EnableInterrupts();
        return P1_INVALID_LOCK;
    }
    if(P1_GetPid() != locks[currentCond->lid].pid){
        P1EnableInterrupts();
        return P1_LOCK_NOT_HELD;
    }
    while(currentCond->numWaiting > 0){
        head = &currentCond->CondQueue;
        curr = head->next; 
        stateVal = P1SetState(curr->pid, P1_STATE_READY, currentCond->lid, vid);
        if(stateVal);
        head->next = curr->next;
        free(curr);
        currentCond->numWaiting--;
        P1Dispatch(FALSE);
    }
    P1EnableInterrupts();
    return result;
}

// This function is a lot like signal, however the lock associated
// with the condition variable does not need to be held by the calling
// process. If there are no processes waiting, do nothing
int P1_NakedSignal(int vid) {
    int result = P1_SUCCESS;
    Condition *currentCond;
    LockQ *curr;
    LockQ prev;
    int stateVal;
    CHECKKERNEL();

    if(vid > P1_MAXCONDS || conditions[vid].inuse == FALSE){
        P1EnableInterrupts();
        return P1_INVALID_COND;
    }
    // more code here
    currentCond = &conditions[vid];
    if(currentCond->numWaiting > 0){
        prev = currentCond->CondQueue;
        curr = prev.next;
        while(NULL != curr->next){
            prev = *curr;
            curr = curr->next;
        }
        currentCond->numWaiting--;
        prev.next = NULL;
        stateVal = P1SetState(curr->pid, P1_STATE_READY, currentCond->lid, vid);
        free((int*)curr->pid);
        free(curr->next);
        P1Dispatch(FALSE);
    }
    return result;
}

// This function copies len characters from the specified lock
// into name
int P1_CondName(int vid, char *name, int len) {
    int result = P1_SUCCESS;
    Condition *currentCond;
    int i;

    CHECKKERNEL();
    if(vid < 0 || vid >= P1_MAXCONDS){
        return P1_INVALID_COND;
    }
    currentCond = &conditions[vid];
    if(NULL == currentCond->name){
        return P1_NAME_IS_NULL;
    }

    for(i = 0; i < len; i++){
        name[i] = currentCond->name[i];
        if(currentCond->name[i] == '\0'){
            break;
        }
    }
    return result;
}
