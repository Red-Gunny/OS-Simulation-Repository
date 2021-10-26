// 2021/10/16(ver1.0) 10/23(ver2.0) / OS / hw2 / B735536 /Geonui HONG
// CPU Schedule Simulator Homework
// Student Number : B735536
// Name : Geonui HONG
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <limits.h>

#define SEED 10

// process states
#define S_IDLE 0			
#define S_READY 1
#define S_BLOCKED 2
#define S_RUNNING 3
#define S_TERMINATE 4

int NPROC, NIOREQ, QUANTUM;

struct ioDoneEvent {
	int procid;
	int doneTime;
	int len;
	struct ioDoneEvent *prev;
	struct ioDoneEvent *next;
} ioDoneEventQueue, *ioDoneEvent;

struct process {
	int id;		
	int len;				
	int targetServiceTime;	
	int serviceTime;
	int startTime;				
	int endTime;				
	char state;			
	int priority;
	int saveReg0, saveReg1;
	struct process *prev;
	struct process *next;
} *procTable;

struct process idleProc;
struct process readyQueue;
struct process blockedQueue;
struct process *runningProc;

int cpuReg0, cpuReg1;
int currentTime = 0;
int *procIntArrTime, *procServTime, *ioReqIntArrTime, *ioServTime;

// ############# customized function List   #############
// ##### to grader : "dont touch under functions" ########
// ############### forward declation ####################
struct process* RRschedule();
struct process* SJFschedule();
struct process* SRTNschedule();
struct process* GSschedule();
struct process* SFSschedule();
 
// whole
void moveProcessFromBlockToReady(struct process* Process);
void moveProcessFromRunningToBlock(struct process* Process);
void moveProcessFromRunningToReady(struct process* Process);
void ioDoneHandler(int doneSize, int currentTime);
void terminateProcess(struct process* Process, int currentTime);
void makeIoEvent(struct process* Process, int requestCount);

// tests
int isForkTime(int nextForkTime);
int isIoTime(int nextIOReqTime, int ioCount);
int isQuantumOver(int qTime);
int isCpuRunning(struct process* runningProcess);
int isGoTerminate(struct process* Process);

// tools
void initQueueHead();
int executeIO();
float getRatio(struct process* Process);
void saveProcessToProcTable(struct process* runningProcess);
void loadProcessFromProcTable(struct process* Process);
void changeProcessState(struct process* Process, char toChangeState);
struct ioDoneEvent* deleteNodeOfIoQueue(struct ioDoneEvent* deleteNode);
struct process* deleteNodeOfProcQueue(struct process* deleteNode, struct process* queueHead);

// creation
struct process* createProcess(int processCount, int currentTime);
int insertProcessToReadyQueue(struct process* Process);

// termination
void recordEndTimeToProcess(struct process* Process, int currentTime);

// IO Req -  Process
int insertProcessToBlockQueue(struct process* Process);

// IO Req - IO Event
int createIoDoneEventForQueue(struct process* Process, int requestCount);
void insertIoDoneEventToQueue(int index);

// IO RES
void ioDoneProcessHandler(struct ioDoneEvent* ioDone, int currentTime);

// ####################################################################
// ####################################################################
// ####################################################################

void compute() {
	// DO NOT CHANGE THIS FUNCTION
	cpuReg0 = cpuReg0 + runningProc->id;
	cpuReg1 = cpuReg1 + runningProc->id;
	//printf("In computer proc %d cpuReg0 %d\n",runningProc->id,cpuReg0);
}

void initProcTable() {
	int i;
	for(i=0; i < NPROC; i++) {
		procTable[i].id = i;
		procTable[i].len = 0;
		procTable[i].targetServiceTime = procServTime[i];
		procTable[i].serviceTime = 0;
		procTable[i].startTime = 0;
		procTable[i].endTime = 0;
		procTable[i].state = S_IDLE;
		procTable[i].priority = 0;
		procTable[i].saveReg0 = 0;
		procTable[i].saveReg1 = 0;
		procTable[i].prev = NULL;
		procTable[i].next = NULL;
	}
}

void procExecSim(struct process *(*scheduler)()) {
	int pid, qTime = 0, cpuUseTime = 0, nproc = 0, termProc = 0, nioreq = 0;
	char schedule = 0, nextState = S_IDLE;
	int nextForkTime, nextIOReqTime;

	initQueueHead();

	runningProc = &idleProc;		// first running IDLE process
	nextForkTime = procIntArrTime[nproc];				// remain time until process forked
	nextIOReqTime = ioReqIntArrTime[nioreq];			// remain time until IO Request

	while (termProc < NPROC) {
		int ioDoneSignal = 0, forkSignal = 0;
		if (runningProc != &idleProc) {
			loadProcessFromProcTable(runningProc);		// load for register compute()
		}
		cpuReg0 = runningProc->saveReg0;
		cpuReg1 = runningProc->saveReg1;
		currentTime++;						// real time
		qTime++;							// quantum timer (for compare)
		runningProc->serviceTime++;			//  now running proc servicetime ++
		if (runningProc != &idleProc) {			// CPU is now running
			cpuUseTime++;					// CPU timer ++
			nextIOReqTime--;
		}
		nextForkTime--;
		// MUST CALL compute() Inside While loop
		compute();		// cpuRegs is changed...
		runningProc->saveReg0 = cpuReg0;		// handling for after compute()
		runningProc->saveReg1 = cpuReg1;
		if (ioDoneEventQueue.len != 0) {		// IO Done handling			 actually.... this same as isIoDoing()   but i wanna hoxy mola error checking..  (hoxy? unmatched pointer & head.len)
			ioDoneSignal = executeIO();
		}
		///////////////////////////// one period  ///////////////////////////////
		if (isCpuRunning(runningProc)) {	// CPU is Real Running => 1st) terminate check.   2nd)  or IO Request check.      3rd)process fork check      4rd) Quantum over check       5rd) io done check 
			saveProcessToProcTable(runningProc); // [!!! FRONT PART !!!] - cuz minimze code's len     about result..   store proc to PCB
			if (isGoTerminate(runningProc)) {	// NOW PROCESS is to TERMINATE ! (first hadling job) 
				struct process* beTerminated = runningProc;	
				if (isIoTime(nextIOReqTime, nioreq)) {
					makeIoEvent(runningProc, nioreq++);			// make IO Event  (Process directyl hadnling X. only read)
					nextIOReqTime = ioReqIntArrTime[nioreq];	// so handling IO Event (request)
				}
				runningProc = &idleProc;
				terminateProcess(beTerminated, currentTime);	//  (1) change process's state (2) record end time (3) store to PCB (4) free (cuz i use malloc) 
				qTime = 0;										// time initialize..  (for next Quantum comparing)
				termProc++;
				if (isForkTime(nextForkTime)) {		// it's a fork time	
					struct process* createdProcess = createProcess(nproc++, currentTime);		//  create process object
					insertProcessToReadyQueue(createdProcess);									// only insert to Ready Q.  and head.len++
					nextForkTime = procIntArrTime[nproc];										// reset the next fork timer
					saveProcessToProcTable(createdProcess);										// now save to PCB
					forkSignal = 1;
				}
				if (ioDoneSignal) {					// IO is now doing
					ioDoneHandler(ioDoneSignal, currentTime);
				}
				runningProc = scheduler();						// call scheduler
				if (runningProc == NULL) {
					runningProc = &idleProc;
				}
				else {
					runningProc->state = S_RUNNING;
					saveProcessToProcTable(runningProc);
				}
			}
			else {	// now running process is not to terminate..
				if (isIoTime(nextIOReqTime, nioreq)) {					// but i wanna make an IO event.
					struct process* beBlock = runningProc;
					makeIoEvent(runningProc, nioreq++);			// make IO Event  (Process directyl hadnling X. only read)
					nextIOReqTime = ioReqIntArrTime[nioreq];
					if (scheduler == SFSschedule) {		// @@@@@@@@@@@@@@ for SFS handling @@@@@@@@@@@@@@@@@
						if (isQuantumOver(qTime)) {		//	quantum over && IO Request
							(beBlock->priority)--;
						}
						else {
							(beBlock->priority)++;			// not complete task..... and IO go.... so ++
						}
					}
					runningProc = &idleProc;
					moveProcessFromRunningToBlock(beBlock);									// change sate & store to PCB & insert blockQ
					if (isForkTime(nextForkTime)) {												// its fork time !
						struct process* createdProcess = createProcess(nproc++, currentTime);		// filled the fields..
						insertProcessToReadyQueue(createdProcess);									// only insert to Ready Q.  and head.len++
						nextForkTime = procIntArrTime[nproc];										// reset the next fork timer
						saveProcessToProcTable(createdProcess);
						forkSignal = 1;
					}
					if (ioDoneSignal) {					// IO is now doing
						ioDoneHandler(ioDoneSignal, currentTime);
					}
					runningProc = scheduler();
					if (runningProc == NULL) {
						runningProc = &idleProc;
					}
					else {
						runningProc->state = S_RUNNING;
						saveProcessToProcTable(runningProc);
					}
					qTime = 0;											 // initialize the quatum timer
				}
				else {	// Process is now running... but not to terminate ... and no IO request...
					if (isQuantumOver(qTime)) {	// Quantum is over (scheduler call o) -> new Proc & IO doing handling and  go to readyQ
						struct process* beReady = runningProc;
						runningProc = &idleProc;
						if (isForkTime(nextForkTime)) {		// its fork time !
							struct process* createdProcess = createProcess(nproc++, currentTime);			// filled the fields..
							insertProcessToReadyQueue(createdProcess);										// only insert to Ready Q.  and head.len++
							nextForkTime = procIntArrTime[nproc];											// reset the next fork timer
							saveProcessToProcTable(createdProcess);
						}
						if (ioDoneSignal) {			// IO is now doing
							ioDoneHandler(ioDoneSignal, currentTime);
						}
						if (scheduler == SFSschedule) {   // @@@@@@@@@@@@@@ SFS handling @@@@@@@@@@@@@@@@@
							(beReady->priority)--;				// QUANTUM IS OVER !!!
						}
						moveProcessFromRunningToReady(beReady);												// change sate & store to PCB & insert blockQ
						runningProc = scheduler();
						if (runningProc == NULL) {
							runningProc = &idleProc;
						}
						else {
							runningProc->state = S_RUNNING;
							saveProcessToProcTable(runningProc);
						}
						qTime = 0;												// Quantum timer initialize ...
					}
					else { // Quantum is not yet~
						if (isForkTime(nextForkTime)) {		// its fork time !
							struct process* createdProcess = createProcess(nproc++, currentTime);					// filled the fields..
							insertProcessToReadyQueue(createdProcess);												// only insert to Ready Q.  and head.len++
							nextForkTime = procIntArrTime[nproc];													// reset the next fork timer
							saveProcessToProcTable(createdProcess);
							forkSignal = 1;
						}
						if (ioDoneSignal) {		// IO is now doing
							ioDoneHandler(ioDoneSignal, currentTime);
						}
						if (forkSignal || ioDoneSignal) {	// now call scheduler...
							struct process* goToReady = runningProc;
							runningProc = &idleProc;
							moveProcessFromRunningToReady(goToReady);								// change sate & store to PCB & insert blockQ
							runningProc = scheduler();
							if (runningProc == NULL) {
								runningProc = &idleProc;
							}
							else {
								runningProc->state = S_RUNNING;
								saveProcessToProcTable(runningProc);
							}
							qTime = 0;													// Quantum timer initialize ...
						}
						else {		//  actually...    nothing... before process continue
						}
					}
				}
			}
		}
		else {	// CPU is IDLE	-> 1st) process all done (but this is loop start part)  or 2nd) io check and 3rd) check the forked time 
			if (isQuantumOver(qTime)) {		// corner case.  not doing  but quantum hadling
				qTime = 0;
			}
			if (isIoTime(nextIOReqTime, nioreq)) {	// ignore IO request
				nextIOReqTime = ioReqIntArrTime[++nioreq];
			}
			if (isForkTime(nextForkTime)) {				// its fork time !
				struct process* createdProcess = createProcess(nproc++, currentTime);		// filled the fields..
				insertProcessToReadyQueue(createdProcess);									// only insert to Ready Q.  and head.len++
				saveProcessToProcTable(createdProcess);
				nextForkTime = procIntArrTime[nproc];										// reset the next fork timer
				forkSignal = 1;
			}
			if (ioDoneSignal) {		// IO is now doing..
				ioDoneHandler(ioDoneSignal, currentTime);
			}
			if (forkSignal || ioDoneSignal) {
				runningProc = scheduler();
				if (runningProc == NULL) {
					runningProc = &idleProc;
				}
				else {
					runningProc->state = S_RUNNING;
					saveProcessToProcTable(runningProc);
				}
				qTime = 0;
			}
		}
	}
}

//RR,SJF(Modified),SRTN,Guaranteed Scheduling(modified),Simple Feed Back Scheduling
struct process* RRschedule() {
	struct process* reference = &readyQueue;
	if (reference->next == &readyQueue) {		// case1 - ReadyQ is empty node
		return NULL;		// ############## MUST check to CPU is end !!!!! ################
	}
	reference = reference->next;			// move to fetch node
	return deleteNodeOfProcQueue(reference, &readyQueue);
}

struct process* SJFschedule() {
	struct process* reference = &readyQueue;
	int targetServiceTime = readyQueue.next->targetServiceTime;
	if (reference->next == &readyQueue) {		// case1 - ReadyQ is empty node
		return NULL;		// ############## MUST check to CPU is end !!!!! ################
	}
	while(reference->next != &readyQueue) {
		reference = reference->next;
		if (targetServiceTime > reference->targetServiceTime) {
			targetServiceTime = reference->targetServiceTime;
		}
	}
	reference = &readyQueue;
	while (reference->next != &readyQueue) {
		reference = reference->next;		// moving
		if (targetServiceTime == reference->targetServiceTime) {  // << Here is now find node to fetch
			return deleteNodeOfProcQueue(reference, &readyQueue);
		}
	}
	printf("SJF() error - LL and tST unmatched\n");
	exit(1);
	return NULL;
}

struct process* SRTNschedule() {
	struct process* reference = &readyQueue;
	int remainingTime = (readyQueue.next->targetServiceTime)-(readyQueue.next->serviceTime);
	if (reference->next == &readyQueue) {		// case1 - ReadyQ is empty node
		return NULL;		// ############## MUST check to CPU is end !!!!! ################
	}
	while (reference->next != &readyQueue) {
		reference = reference->next;
		if (remainingTime > (reference->targetServiceTime - reference->serviceTime)) {
			remainingTime = reference->targetServiceTime - reference->serviceTime;
		}
	}
	reference = &readyQueue;
	while (reference->next != &readyQueue) {
		reference = reference->next;		// moving
		if (remainingTime == (reference->targetServiceTime - reference->serviceTime)) {  // << Here is now find node to fetch
			return deleteNodeOfProcQueue(reference, &readyQueue);
		}
	}
	printf("SRTN() error - LL and RemainTime unmatched\n");
	exit(1);
	return NULL;
}

struct process* GSschedule() {
	struct process* reference = &readyQueue;
	float ratio = -1;
	int i = 0;
	if (reference->next == &readyQueue) {		// case1 - ReadyQ is empty node
		return NULL;		// ############## MUST check to CPU is end !!!!! ################
	}
	ratio = getRatio(reference->next);
	while (reference->next != &readyQueue) {
		reference = reference->next;
		if (ratio > getRatio(reference)) {
			ratio = getRatio(reference);
		}
	}
	reference = &readyQueue;
	while (reference->next != &readyQueue) {
		reference = reference->next;		// moving
		if (ratio == getRatio(reference)) {  // << Here is now find node to fetch
			return deleteNodeOfProcQueue(reference, &readyQueue);
		}
	}
	printf("GS() error - LL and RemainTime unmatched\n");
	exit(1);
	return NULL;
}

/**
*	 first  Find at PCB  ......   and fetch from the ReadyQ
*/
struct process* SFSschedule() {
	struct process* reference = &readyQueue;
	int priority = readyQueue.next->priority;
	if (reference->next == &readyQueue) {
		return NULL;
	}
	while (reference->next != &readyQueue) {
		reference = reference->next;
		if (priority < reference->priority) {
			priority = reference->priority;
		}
	}
	reference = &readyQueue;
	while (reference->next != &readyQueue) {
		reference = reference->next;		// moving
		if (priority == reference->priority) {  // << Here is now find node to fetch
			return deleteNodeOfProcQueue(reference, &readyQueue);
		}
	}
	printf("SFS() error - LL & priority unmatched\n");
	exit(1);
	return NULL;
}

void printResult() {
	// DO NOT CHANGE THIS FUNCTION
	int i;
	long totalProcIntArrTime=0,totalProcServTime=0,totalIOReqIntArrTime=0,totalIOServTime=0;
	long totalWallTime=0, totalRegValue=0;
	for(i=0; i < NPROC; i++) {
		totalWallTime += procTable[i].endTime - procTable[i].startTime;
		/*
		printf("proc %d serviceTime %d targetServiceTime %d saveReg0 %d\n",
			i,procTable[i].serviceTime,procTable[i].targetServiceTime, procTable[i].saveReg0);
		*/
		totalRegValue += procTable[i].saveReg0+procTable[i].saveReg1;
		/* printf("reg0 %d reg1 %d totalRegValue %d\n",procTable[i].saveReg0,procTable[i].saveReg1,totalRegValue);*/
	}
	for(i = 0; i < NPROC; i++ ) { 
		totalProcIntArrTime += procIntArrTime[i];
		totalProcServTime += procServTime[i];
	}
	for(i = 0; i < NIOREQ; i++ ) { 
		totalIOReqIntArrTime += ioReqIntArrTime[i];
		totalIOServTime += ioServTime[i];
	}
	
	printf("Avg Proc Inter Arrival Time : %g \tAverage Proc Service Time : %g\n", (float) totalProcIntArrTime/NPROC, (float) totalProcServTime/NPROC);
	printf("Avg IOReq Inter Arrival Time : %g \tAverage IOReq Service Time : %g\n", (float) totalIOReqIntArrTime/NIOREQ, (float) totalIOServTime/NIOREQ);
	printf("%d Process processed with %d IO requests\n", NPROC,NIOREQ);
	printf("Average Wall Clock Service Time : %g \tAverage Two Register Sum Value %g\n", (float) totalWallTime/NPROC, (float) totalRegValue/NPROC);
	
}

int main(int argc, char *argv[]) {
	// DO NOT CHANGE THIS FUNCTION
	int i;
	int totalProcServTime = 0, ioReqAvgIntArrTime;
	int SCHEDULING_METHOD, MIN_INT_ARRTIME, MAX_INT_ARRTIME, MIN_SERVTIME, MAX_SERVTIME, MIN_IO_SERVTIME, MAX_IO_SERVTIME, MIN_IOREQ_INT_ARRTIME;
	
	if (argc < 12) {
		printf("%s: SCHEDULING_METHOD NPROC NIOREQ QUANTUM MIN_INT_ARRTIME MAX_INT_ARRTIME MIN_SERVTIME MAX_SERVTIME MIN_IO_SERVTIME MAX_IO_SERVTIME MIN_IOREQ_INT_ARRTIME\n",argv[0]);
		exit(1);
	}
	
	SCHEDULING_METHOD = atoi(argv[1]);
	NPROC = atoi(argv[2]);
	NIOREQ = atoi(argv[3]);
	QUANTUM = atoi(argv[4]);
	MIN_INT_ARRTIME = atoi(argv[5]);
	MAX_INT_ARRTIME = atoi(argv[6]);
	MIN_SERVTIME = atoi(argv[7]);
	MAX_SERVTIME = atoi(argv[8]);
	MIN_IO_SERVTIME = atoi(argv[9]);
	MAX_IO_SERVTIME = atoi(argv[10]);
	MIN_IOREQ_INT_ARRTIME = atoi(argv[11]);
	
	printf("SIMULATION PARAMETERS : SCHEDULING_METHOD %d NPROC %d NIOREQ %d QUANTUM %d \n", SCHEDULING_METHOD, NPROC, NIOREQ, QUANTUM);
	printf("MIN_INT_ARRTIME %d MAX_INT_ARRTIME %d MIN_SERVTIME %d MAX_SERVTIME %d\n", MIN_INT_ARRTIME, MAX_INT_ARRTIME, MIN_SERVTIME, MAX_SERVTIME);
	printf("MIN_IO_SERVTIME %d MAX_IO_SERVTIME %d MIN_IOREQ_INT_ARRTIME %d\n", MIN_IO_SERVTIME, MAX_IO_SERVTIME, MIN_IOREQ_INT_ARRTIME);
	
	srandom(SEED);
	
	// allocate array structures
	procTable = (struct process *) malloc(sizeof(struct process) * NPROC);	
	ioDoneEvent = (struct ioDoneEvent *) malloc(sizeof(struct ioDoneEvent) * NIOREQ);	// this can hold ioDoneEvent variable not pointer
	procIntArrTime = (int *) malloc(sizeof(int) * NPROC);
	procServTime = (int *) malloc(sizeof(int) * NPROC);
	ioReqIntArrTime = (int *) malloc(sizeof(int) * NIOREQ);
	ioServTime = (int *) malloc(sizeof(int) * NIOREQ);

	// initialize queues
	readyQueue.next = readyQueue.prev = &readyQueue;
	
	blockedQueue.next = blockedQueue.prev = &blockedQueue;
	ioDoneEventQueue.next = ioDoneEventQueue.prev = &ioDoneEventQueue;
	ioDoneEventQueue.doneTime = INT_MAX;
	ioDoneEventQueue.procid = -1;
	ioDoneEventQueue.len  = readyQueue.len = blockedQueue.len = 0;
	
	// generate process interarrival times
	for(i = 0; i < NPROC; i++ ) { 
		procIntArrTime[i] = random()%(MAX_INT_ARRTIME - MIN_INT_ARRTIME+1) + MIN_INT_ARRTIME;
	}
	
	// assign service time for each process
	for(i=0; i < NPROC; i++) {
		procServTime[i] = random()% (MAX_SERVTIME - MIN_SERVTIME + 1) + MIN_SERVTIME;
		totalProcServTime += procServTime[i];	
	}
	
	ioReqAvgIntArrTime = totalProcServTime/(NIOREQ+1);
	assert(ioReqAvgIntArrTime > MIN_IOREQ_INT_ARRTIME);
	
	// generate io request interarrival time
	for(i = 0; i < NIOREQ; i++ ) { 
		ioReqIntArrTime[i] = random()%((ioReqAvgIntArrTime - MIN_IOREQ_INT_ARRTIME)*2+1) + MIN_IOREQ_INT_ARRTIME;
	}
	
	// generate io request service time
	for(i = 0; i < NIOREQ; i++ ) { 
		ioServTime[i] = random()%(MAX_IO_SERVTIME - MIN_IO_SERVTIME+1) + MIN_IO_SERVTIME;
	}
	
#ifdef DEBUG
	// printing process interarrival time and service time
	printf("Process Interarrival Time :\n");
	for(i = 0; i < NPROC; i++ ) { 
		printf("%d ",procIntArrTime[i]);
	}
	printf("\n");
	printf("Process Target Service Time :\n");
	for(i = 0; i < NPROC; i++ ) { 
		printf("%d ",procTable[i].targetServiceTime);
	}
	printf("\n");
#endif
	
	// printing io request interarrival time and io request service time
	printf("IO Req Average InterArrival Time %d\n", ioReqAvgIntArrTime);
	printf("IO Req InterArrival Time range : %d ~ %d\n",MIN_IOREQ_INT_ARRTIME,
			(ioReqAvgIntArrTime - MIN_IOREQ_INT_ARRTIME)*2+ MIN_IOREQ_INT_ARRTIME);
			
#ifdef DEBUG		
	printf("IO Req Interarrival Time :\n");
	for(i = 0; i < NIOREQ; i++ ) { 
		printf("%d ",ioReqIntArrTime[i]);
	}
	printf("\n");
	printf("IO Req Service Time :\n");
	for(i = 0; i < NIOREQ; i++ ) { 
		printf("%d ",ioServTime[i]);
	}
	printf("\n");
#endif
	
	struct process* (*schFunc)();
	switch(SCHEDULING_METHOD) {
		case 1 : schFunc = RRschedule; break;
		case 2 : schFunc = SJFschedule; break;
		case 3 : schFunc = SRTNschedule; break;
		case 4 : schFunc = GSschedule; break;
		case 5 : schFunc = SFSschedule; break;
		default : printf("ERROR : Unknown Scheduling Method\n"); exit(1);
	}
	initProcTable();
	procExecSim(schFunc);
	printResult();
}

/**
 * this section is I wrote 
 */
 ///////////////////////////////////// Tools ///////////////////////////////////
// when GS scheduler..
float getRatio(struct process* Process) {
	return (float)Process->serviceTime / Process->targetServiceTime;
}

/**
*  Initializeing the ReadyQ, BlockQ.
* (caution) must be called before execution !!!!
*/
void initQueueHead() {
	readyQueue.id = -1;
	blockedQueue.id = -1;
}

void moveProcessFromRunningToReady(struct process* Process) {
	changeProcessState(Process, S_READY);
	saveProcessToProcTable(Process);
	insertProcessToReadyQueue(Process);
}

int executeIO() {
	struct ioDoneEvent* reference = &ioDoneEventQueue;
	int doneCount = 0;
	while (reference->next != &ioDoneEventQueue) {
		reference = reference->next;
		reference->doneTime--;
		if (reference->doneTime == 0) {
			doneCount++;
		}
	}
	return doneCount;
}

void changeProcessState(struct process* Process, char toChangeState) {
	Process->state = toChangeState;
}

/**
*	save process information to PCB
*/
void saveProcessToProcTable(struct process* runningProcess) {
	int pid = runningProcess->id;
	procTable[pid].targetServiceTime = runningProcess->targetServiceTime;
	procTable[pid].serviceTime = runningProcess->serviceTime;
	procTable[pid].startTime = runningProcess->startTime;
	procTable[pid].endTime = runningProcess->endTime;
	procTable[pid].state = runningProcess->state;
	procTable[pid].priority = runningProcess->priority;
	procTable[pid].saveReg0 = runningProcess->saveReg0;
	procTable[pid].saveReg1 = runningProcess->saveReg1;
}

// this is inverse
void loadProcessFromProcTable(struct process* Process) {
	if (procTable[Process->id].state == S_TERMINATE) {
		printf("load error() - state is TERMINATE !!\n");
		exit(1);
	}
	Process->id = procTable[Process->id].id;
	Process->serviceTime = procTable[Process->id].serviceTime;
	Process->state = procTable[Process->id].state;
	Process->priority = procTable[Process->id].priority;
	Process->saveReg0 = procTable[Process->id].saveReg0;
	Process->saveReg1 = procTable[Process->id].saveReg1;
	Process->prev = NULL;
	Process->next = NULL;
}

/**
*   Delete 'A' Node of "Proc" Queue		(like ReadyQ, BlockQ)
*/
struct process* deleteNodeOfProcQueue(struct process* deleteNode, struct process* queueHead) {
	struct process* prevNode = deleteNode->prev;
	struct process* nextNode = deleteNode->next;
	if (prevNode == NULL || nextNode == NULL) {
		printf("deleteNodeOfProcQueue() error - prevNode or nextNode is NULL!! \n");
		exit(1);
	}
	if ((prevNode == queueHead) && (nextNode == queueHead)) {		// case1	
		queueHead->next = queueHead->prev = queueHead;
	}
	else if ((prevNode == queueHead) && (nextNode != queueHead)) {
		if (nextNode == NULL) { printf("deleteNodeOfProcQueue() error - nextNode is NULL\n"); exit(1); };
		queueHead->next = nextNode;
		nextNode->prev = queueHead;
	}
	else if ((prevNode != queueHead) && (nextNode == queueHead)) {
		if (prevNode == NULL) { printf("deleteNodeOfProcQueue() error - prevNode is NULL\n"); exit(1); };
		queueHead->prev = prevNode;
		prevNode->next = queueHead;
	}
	else if ((prevNode != queueHead) && (nextNode != queueHead)) {
		if (prevNode == NULL || nextNode == NULL) { printf("deleteNodeOfProcQueue() error - sideNodes is NULL\n"); exit(1); };
		prevNode->next = nextNode;
		nextNode->prev = prevNode;
	}
	else {
		printf("deleteNodeOfProcQueue() error - case error \n");
		exit(1);
	}
	deleteNode->prev = NULL;
	deleteNode->next = NULL;
	(queueHead->len)--;
	return deleteNode;
}

/**
*   Delete 'A' Node of "IO" Queue
*/
struct ioDoneEvent* deleteNodeOfIoQueue(struct ioDoneEvent* deleteNode) {
	struct ioDoneEvent* prevNode = deleteNode->prev;
	struct ioDoneEvent* nextNode = deleteNode->next;
	if (prevNode == NULL || nextNode == NULL) {
		printf("deleteNodeOfIoQueue() error - prevNode or nextNode is NULL!! \n");
		exit(1);
	}
	if ((prevNode == &ioDoneEventQueue) && (nextNode == &ioDoneEventQueue)) {		// case1	
		ioDoneEventQueue.next = ioDoneEventQueue.prev = &ioDoneEventQueue;
	}
	else if ((prevNode == &ioDoneEventQueue) && (nextNode != &ioDoneEventQueue)) {
		if (nextNode == NULL) { printf("deleteNodeOfIoQueue() error - nextNode is NULL\n"); exit(1); };
		ioDoneEventQueue.next = nextNode;
		nextNode->prev = &ioDoneEventQueue;
	}
	else if ((prevNode != &ioDoneEventQueue) && (nextNode == &ioDoneEventQueue)) {
		if (prevNode == NULL) { printf("deleteNodeOfIoQueue() error - prevNode is NULL\n"); exit(1); };
		ioDoneEventQueue.prev = prevNode;
		prevNode->next = &ioDoneEventQueue;
	}
	else if ((prevNode != &ioDoneEventQueue) && (nextNode != &ioDoneEventQueue)) {
		if (prevNode == NULL || nextNode == NULL) { printf("deleteNodeOfIoQueue() error - sideNodes is NULL\n"); exit(1); };
		prevNode->next = nextNode;
		nextNode->prev = prevNode;
	}
	else {
		printf("deleteNodeOfIoQueue() error - case error \n");
		exit(1);
	}
	deleteNode->prev = NULL;
	deleteNode->next = NULL;
	(ioDoneEventQueue.len)--;
	return deleteNode;
}


 //////////////////////////// Process Createion ////////////////////////////////
/**
* real make Process
* [caution] parameter processCount must exists in procExecSim() !!!!!!
*/
struct process* createProcess(int processCount, int currentTime) {
	struct process* createdProcess = (struct process*)malloc(sizeof(struct process));
	createdProcess->id = processCount;
	createdProcess->len = processCount+1;
	createdProcess->targetServiceTime = procServTime[processCount++];	
	createdProcess->serviceTime = 0;	
	createdProcess->startTime = currentTime;	
	createdProcess->endTime = 0;			
	createdProcess->state = S_READY;		
	createdProcess->priority = 0;
	createdProcess->saveReg0 = 0;
	createdProcess->saveReg1 = 0;
	createdProcess->prev = NULL;
	createdProcess->next = NULL;
	return createdProcess;
}

/**
* insert Process the ReadyQueue
*/
int  insertProcessToReadyQueue(struct process* Process) {
	struct process* reference = &readyQueue;	// pointing to LL Head
	Process->next = &readyQueue;		//  NEW -> next 
	reference = reference->prev;			// (new added to optimize) moiving to before->next
	reference->next = Process;				//(new added to optimize) before -> next
	Process->prev = reference;				//(new added to optimize)  NEW -> prev
	readyQueue.prev = Process;			// head -> prev
	return ++(readyQueue.len);
}

////////////////////////// Process Termination ////////////////////////////////
/**
*  about Termination - whole handling
*  like Configure()
*/
void terminateProcess(struct process* Process, int currentTime) {
	changeProcessState(Process, S_TERMINATE);		// (1) Change Process's State
	recordEndTimeToProcess(Process, currentTime);	// (2) Record Current Time at Process	
	saveProcessToProcTable(Process);				// (3) save Process to PCB
	free(Process);							// (4) free (cuz I use malloc) 
}

/**
* (2) record now time at the process
*/
void recordEndTimeToProcess(struct process* Process, int currentTime) {
	Process->endTime = currentTime;
}

///////////////// Context Switch : (Running -> Block) /////////////////////////
/**
*  (Context Switch - cuz IO)  about only Process - whole handling
*  like Configure()
*/
void moveProcessFromRunningToBlock(struct process* Process) {
	changeProcessState(Process, S_BLOCKED);		// (1) Process's state transition (run->block)
	saveProcessToProcTable(Process);			// (2) save process information 
	insertProcessToBlockQueue(Process);			// (3)  insert the process to BlockQueue
}

/**
*	insert process to BlockQueue
*/
int insertProcessToBlockQueue(struct process* Process) {
	struct process* reference = &blockedQueue;	// pointing to LL Head
	Process->next = &blockedQueue;		//  NEW -> next 
	reference = reference->prev;			// (new added to optimize) moiving to before->next
	reference->next = Process;				//(new added to optimize) before -> next
	Process->prev = reference;				//(new added to optimize)  NEW -> prev
	blockedQueue.prev = Process;			// head -> prev
	return ++(blockedQueue.len);
}

///////////////// System Call : make IO Request Event /////////////////////////
// test suceecss -- LL make well
void makeIoEvent(struct process* Process, int requestCount) {
	struct ioDoneEvent* reference = &ioDoneEventQueue;	// pointing to LL Head
	reference = reference->prev;		// the last node
	ioDoneEvent[requestCount].procid = Process->id;
	ioDoneEvent[requestCount].doneTime = ioServTime[requestCount];
	ioDoneEvent[requestCount].len = 1;			// for seperate to ignore case  (when io arrived at CPU IDLE)
	ioDoneEvent[requestCount].prev = reference;				// New -> prev 
	ioDoneEvent[requestCount].next = &ioDoneEventQueue;		// NEW -> next
	reference->next = &ioDoneEvent[requestCount];			// before->next
	ioDoneEventQueue.prev = &ioDoneEvent[requestCount];		// head->prev
	(ioDoneEventQueue.len)++;
}

///////////////////// Interruipt : IO Done interrupt  /////////////////////////
void ioDoneHandler(int doneSize, int currentTime) {
	struct ioDoneEvent* referenceIO = &ioDoneEventQueue;
	int doneCount = 0;
	referenceIO = referenceIO->next;
	while (referenceIO != &ioDoneEventQueue) {				// moving
		if (doneCount == doneSize) {
			break;
		}
 		if (referenceIO->doneTime == 0) {				// now find the DONE IO
			struct ioDoneEvent* toDelete = referenceIO;
			referenceIO = referenceIO->next;
			toDelete = deleteNodeOfIoQueue(toDelete);		// that fetch
			ioDoneProcessHandler(toDelete, currentTime);		// about process handling
			doneCount++;
			continue;
		}
		referenceIO = referenceIO->next;
	}
}

void ioDoneProcessHandler(struct ioDoneEvent* ioDone, int currentTime) {
	struct process* ref = &blockedQueue;
	while (ref->next != &blockedQueue) {
		ref = ref->next;
		if (ref->id == ioDone->procid) {		// now find
			ref = deleteNodeOfProcQueue(ref, &blockedQueue);
			if (ref->serviceTime != ref->targetServiceTime) {
				moveProcessFromBlockToReady(ref);
			}
			else {
				terminateProcess(ref, currentTime);
			}
			break;
		}
	}
}

/**
* literally ...  move "process" from blockQ to readyQ
*/
void moveProcessFromBlockToReady(struct process* Process) {
	struct process* ref = &readyQueue;
	changeProcessState(Process, S_READY);		// (1) change the process's state field 
	saveProcessToProcTable(Process);			// (2) copy to PCB . ### ???ì¿????? ?ì¼??À§ ??????Æ®
	insertProcessToReadyQueue(Process);			// (3) now insert the process to readyQ
}

///////////////////////////////////// Tests ///////////////////////////////////
int isForkTime(int nextForkTime) {
	if (nextForkTime == 0) {
		return 1;
	}
	else {
		return 0;
	}
}

int isIoTime(int nextIOReqTime, int ioCount) {
	if (ioCount >= NIOREQ) {
		return 0;
	}
	if (nextIOReqTime == 0) {
		return 1;
	}
	else {
		return 0;
	}
}

int isQuantumOver(int qTime) {
	if (qTime == QUANTUM) {
		return 1;
	}
	else {
		return 0;
	}
}

int isCpuRunning(struct process* runningProcess) {
	if (runningProcess != &idleProc) {
		return 1;
	}
	else {
		return 0;
	}
}

int isGoTerminate(struct process* Process) {
	if (Process->serviceTime == Process->targetServiceTime) {
		return 1;
	}
	else {
		return 0;
	}
}