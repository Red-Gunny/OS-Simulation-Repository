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
// ##### to grader : "dont touch under functions" ########		-- Random() function check !!!!
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
int ioHandler();
void terminateProcess(struct process* Process, int currentTime);
void makeIoEvent(struct process* Process, int requestCount);

// tests
int isForkTime(int nextForkTime);
int isIoTime(int nextIOReqTime);
int isQuantumOver(int qTime);
int isCpuRunning(struct process* runningProcess);
int isGoTerminate(struct process* Process);
int isIoDoing();
int isAllDone();

// tools
void initQueueHead();
void executeIO();
float getRatio(struct process* Process);
void saveProcessToProcTable(struct process* runningProcess);
void loadProcessFromProcTable(struct process* Process);
void changeProcessState(struct process* Process, char toChangeState);
struct ioDoneEvent* deleteNodeOfIoQueue(struct ioDoneEvent* deleteNode);
struct process* deleteNodeOfProcQueue(struct process* deleteNode, struct process* queueHead);

// creation
int isCreateTime(int nextForkTime);
struct process* createProcess(int processCount, int currentTime);
int insertProcessToReadyQueue(struct process* Process);

// termination
void recordEndTimeToProcess(struct process* Process, int currentTime);
void freeProcess(struct process* Process);

// IO Req -  Process
int insertProcessToBlockQueue(struct process* Process);

// IO Req - IO Event
struct ioDoneEvent* createIoDoneEventForQueue(struct process* Process, int requestCount);
void insertIoDoneEventToQueue(struct ioDoneEvent* createdIoDoneEvent);

// IO Done
void initIoDoneEventArr(int NIOREQ);
void interruptServiceRoutine(int* bitVector, int vectorSize, int currentTime);
int* checkIoIsDone(int* bitVector, int ioQueSize);
void emitInterrupt(int* bitVector, int ioQueSize);
void deleteIoDoneEvent(int* bitVector, int vectorSize);
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

	while (!isAllDone()) {
		int ioSignal = 0, forkSignal = 0;
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
			executeIO();
		}
		///////////////////////////// one period  ///////////////////////////////
		if (isCpuRunning(runningProc)) {	// CPU is Real Running => 1st) terminate check.   2nd)  or IO Request check.      3rd)process fork check      4rd) Quantum over check       5rd) io done check 
			saveProcessToProcTable(runningProc); // [!!! FRONT PART !!!] - cuz minimze code's len     about result..   store proc to PCB
			if (isGoTerminate(runningProc)) {	// NOW PROCESS is to TERMINATE ! (first hadling job) 
				struct process* beTerminated = runningProc;	
				if (isIoTime(nextIOReqTime)) {
					makeIoEvent(runningProc, nioreq++);			// make IO Event  (Process directyl hadnling X. only read)
					nextIOReqTime = ioReqIntArrTime[nioreq];	// so handling IO Event (request)
				}
				runningProc = &idleProc;
				terminateProcess(beTerminated, currentTime);	//  (1) change process's state (2) record end time (3) store to PCB (4) free (cuz i use malloc) 
				qTime = 0;										// time initialize..  (for next Quantum comparing)
				if (isForkTime(nextForkTime)) {		// it's a fork time	
					struct process* createdProcess = createProcess(nproc++, currentTime);		//  create process object
					insertProcessToReadyQueue(createdProcess);									// only insert to Ready Q.  and head.len++
					nextForkTime = procIntArrTime[nproc];										// reset the next fork timer
					saveProcessToProcTable(createdProcess);										// now save to PCB
					forkSignal = 1;
				}
				if (isIoDoing()) {					// IO is now doing
					ioSignal = ioHandler();
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
				if (isIoTime(nextIOReqTime)) {					// but i wanna make an IO event.
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
					if (isIoDoing()) {					// IO is now doing
						ioSignal = ioHandler();
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
						if (isIoDoing()) {			// IO is now doing
							ioSignal = ioHandler();
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
						int forkCheck = 0, ioDoneCheck = 0;
						if (isForkTime(nextForkTime)) {		// its fork time !
							struct process* createdProcess = createProcess(nproc++, currentTime);					// filled the fields..
							insertProcessToReadyQueue(createdProcess);												// only insert to Ready Q.  and head.len++
							nextForkTime = procIntArrTime[nproc];													// reset the next fork timer
							saveProcessToProcTable(createdProcess);
							forkCheck = 1;
						}
						if (isIoDoing()) {		// IO is now doing
							ioDoneCheck = ioHandler();
						}
						if (forkCheck || ioDoneCheck) {	// now call scheduler...
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
			if (isIoTime(nextIOReqTime)) {	// ignore IO request
				nextIOReqTime = ioReqIntArrTime[++nioreq];
			}
			if (isForkTime(nextForkTime)) {				// its fork time !
				struct process* createdProcess = createProcess(nproc++, currentTime);		// filled the fields..
				insertProcessToReadyQueue(createdProcess);									// only insert to Ready Q.  and head.len++
				saveProcessToProcTable(createdProcess);
				nextForkTime = procIntArrTime[nproc];										// reset the next fork timer
				forkSignal = 1;
			}
			if (isIoDoing()) {		// IO is now doing..
				ioSignal = ioHandler();
			}
			if (forkSignal || ioSignal) {
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
	int targetServiceTime = -1;
	int i = 0;
	if (reference->next == &readyQueue) {		// case1 - ReadyQ is empty node
		if (readyQueue.len != 0) {
			printf("SJF() error ! - check readyQueue's len !!\n"); exit(1);	// for my error check
		}
		return NULL;		// ############## MUST check to CPU is end !!!!! ################
	}
	while (targetServiceTime == -1) {			// setting BenchMark
		if (procTable[i].state == S_READY) {
			targetServiceTime = procTable[i].targetServiceTime;
		}
		i++;
	}
	for (i = 0; i < NPROC; i++) {				// find what is shortest job from PT 
		if (procTable[i].state == S_READY) {		// but only find  "targetServiceTime"
			if (targetServiceTime > procTable[i].targetServiceTime) {
				targetServiceTime = procTable[i].targetServiceTime;
			}
		}
	}
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
	int remainingTime = -1;
	int i = 0;
	if (reference->next == &readyQueue) {		// case1 - ReadyQ is empty node
		if (readyQueue.len != 0) {
			printf("SJF() error ! - check readyQueue's len !!\n"); exit(1);	// for my error check
		}
		return NULL;		// ############## MUST check to CPU is end !!!!! ################
	}
	while (remainingTime == -1) {			// setting BenchMark
		if (procTable[i].state == S_READY) {
			remainingTime = procTable[i].targetServiceTime - procTable[i].serviceTime;
		}
		i++;
	}
	for (i = 0; i < NPROC; i++) {				// find what is shortest job from PT 
		if (procTable[i].state == S_READY) {		// but only find  "targetServiceTime"
			if (remainingTime > (procTable[i].targetServiceTime - procTable[i].serviceTime)) {
				remainingTime = procTable[i].targetServiceTime - procTable[i].serviceTime;
			}
		}
	}
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
		if (readyQueue.len != 0) {
			printf("GS() error ! - check readyQueue's len !!\n"); exit(1);	// for my error check
		}
		return NULL;		// ############## MUST check to CPU is end !!!!! ################
	}
	while (ratio == -1) {						// setting BenchMark
		if (procTable[i].state == S_READY) {
			ratio = getRatio(&procTable[i]);
		}
		i++;
	}
	for (i = 0; i < NPROC; i++) {				// find what is the smallest RATIO from PT 
		if (procTable[i].state == S_READY) {
			if (ratio > getRatio(&procTable[i])) {
				ratio = getRatio(&procTable[i]);
			}
		}
	}
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
* [[[[caution !]]]]]]  MUST update - about priority !!!
*/
struct process* SFSschedule() {
	struct process* reference = &readyQueue;
	int priority;
	int i = 0;
	int checkError = 1;
	int isReadyProc = 0;
	if (reference->next == &readyQueue) {
		if (readyQueue.len != 0) {
			printf("SFS error! = check readyQueue's len !! \n"); exit(1);
		}
		return NULL;
	}
	for (i = 0; i < NPROC; i++) {			// setting benchmark
		if (procTable[i].state == S_READY || procTable[i].state == S_RUNNING) {		// if this scheduler() called  - means Ready Q already exists  node !!!!!!!
			priority = procTable[i].priority;
			checkError = 0;
			break;
		}
	}
	if (checkError) {
		printf("SFS() error - S_READY or RUUNNING is not exists... in the PT\n");
		exit(1);
	}
	for (i = 0; i < NPROC; i++) {
		if (procTable[i].state == S_READY) {
			if (priority < procTable[i].priority) {
				priority = procTable[i].priority;
				if (procTable[i].state == S_RUNNING) {			// actually dont need. cuze caller  hadling this part..  but hoxy mola..
					isReadyProc = 1;
				}
				else {
					isReadyProc = 0;
				}
			}
		}
	}
	if (isReadyProc) {	// hoxy mola~
		return NULL;
	}
	while (reference->next != &readyQueue) {
		reference = reference->next;
		if (priority == reference->priority) {
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
//////////////////////////////////////////
// insert.. when debugging
///////////////////////////////////////////

// Quantum is done
void moveProcessFromRunningToReady(struct process* Process) {
	changeProcessState(Process, S_READY);
	saveProcessToProcTable(Process);
	insertProcessToReadyQueue(Process);
}

 // real funtion extration
int ioHandler() {
	//int arr[arrSize];
	int* ioArr = (int*)malloc(sizeof(int) * ioDoneEventQueue.len); //procServTime = (int*)malloc(sizeof(int) * NPROC);
	int ioArrLen = ioDoneEventQueue.len;
	int interruptSignal = 0;
	initIoDoneEventArr(NIOREQ);
	ioArr = checkIoIsDone(ioArr, ioDoneEventQueue.len);
	if (ioArr[0] != -1) {
		emitInterrupt(ioArr, ioDoneEventQueue.len);
		deleteIoDoneEvent(ioArr, ioDoneEventQueue.len);
		interruptServiceRoutine(ioArr, ioArrLen, currentTime);
		interruptSignal = 1;
	}
	free(ioArr);
	return interruptSignal;
}

void executeIO() {
	struct ioDoneEvent* reference = &ioDoneEventQueue;
	while (reference->next != &ioDoneEventQueue) {
		reference = reference->next;
		reference->doneTime--;
	}
}

 ///////////////////////////////////////////////////////////////////////////////
 ///////////////////////////////////// Tests ///////////////////////////////////
 ///////////////////////////////////////////////////////////////////////////////
int isForkTime(int nextForkTime) {
	if (nextForkTime == 0) {
		return 1;
	}
	else {
		return 0;
	}
}

int isIoTime(int nextIOReqTime) {
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

int isIoDoing() {
	struct ioDoneEvent* refernce = &ioDoneEventQueue;
	if (refernce->next != &ioDoneEventQueue) {	
		return 1;
	}
	else {
		if (ioDoneEventQueue.len != 0) { printf("IOQue & Head.len is not synchronized\n"); exit(1); }
		return 0;
	}
}

int isAllDone() {
	int i = 0;
	for (i = 0; i < NPROC; i++) {
		if (procTable[i].state != S_TERMINATE) {
			return 0;
		}
	}
	return 1;
}

 ///////////////////////////////////////////////////////////////////////////////
 ///////////////////////////////////// Tools ///////////////////////////////////
 ///////////////////////////////////////////////////////////////////////////////

// when GS scheduler..
float getRatio(struct process* Process) {
	return (float)Process->serviceTime / Process->targetServiceTime;
}

/**
* [Test Success !!]
*  Initializeing the ReadyQ, BlockQ.
* (caution) must be called before execution !!!!
*/
void initQueueHead() {
	readyQueue.id = -1;
	blockedQueue.id = -1;
}

/**
	[test success !!]
	Process's state transition method
*/
void changeProcessState(struct process* Process, char toChangeState) {
	Process->state = toChangeState;
}

/**
*  [test success !!]
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
* [test success !!]
*   Delete 'A' Node of "IO" Queue 
*   logical error x  maybe... 
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

/**
* [test success !!]
*   Delete 'A' Node of "Proc" Queue		(like ReadyQ, BlockQ)
*   logical error x  maybe...
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

 ///////////////////////////////////////////////////////////////////////////////
 //////////////////////////// Process Createion ////////////////////////////////
 ///////////////////////////////////////////////////////////////////////////////
 /**
 * [test Success !!]
 * (1) check the process create time
 */
int isCreateTime(int nextForkTime) {
	if (nextForkTime == 0) {
		return 1;
	}
	else {
		return 0;
	}
}

/**
* [Test Success !!]
* (2) real make Process
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
* [Test - Sucess !!]
* (3) insert Process the ReadyQueue
*/
int  insertProcessToReadyQueue(struct process* Process) {
	struct process* reference = &readyQueue;	// pointing to LL Head
	if (reference == NULL) {
		printf("Initial ReadyQueue's Head is NULL!! \n");
		exit(1);
	}
	Process->next = &readyQueue;		//  NEW -> next 
	readyQueue.prev = Process;			// head -> prev
	while (reference->next != &readyQueue) {
		reference = reference->next;		// moving
	}
	reference->next = Process;					//  before -> next
	Process->prev = reference;				// NEW -> prev
	return ++(readyQueue.len);
}

///////////////////////////////////////////////////////////////////////////////
////////////////////////// Process Termination ////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/**
* [Test Success !!]
*  about Termination - whole handling
*  like Configure()
*/
void terminateProcess(struct process* Process, int currentTime) {
	changeProcessState(Process, S_TERMINATE);		// (1) Change Process's State
	recordEndTimeToProcess(Process, currentTime);	// (2) Record Current Time at Process	
	saveProcessToProcTable(Process);				// (3) save Process to PCB
	freeProcess(Process);							// (4) free (cuz I use malloc) 
}

/**
* [Test Success !!]
* (2) record now time at the process
*/
void recordEndTimeToProcess(struct process* Process, int currentTime) {
	Process->endTime = currentTime;
}

/**
* [Test Success !!]
* (4) free ( cuz malloc() )
*/
void freeProcess(struct process* Process) {
	free(Process);
}

///////////////////////////////////////////////////////////////////////////////
///////////////// Context Switch : (Running -> Block) /////////////////////////
///////////////////////////////////////////////////////////////////////////////
/**
* [Test Success !!]
*  (Context Switch - cuz IO)  about only Process - whole handling
*  like Configure()
*/
void moveProcessFromRunningToBlock(struct process* Process) {
	changeProcessState(Process, S_BLOCKED);		// (1) Process's state transition (run->block)
	saveProcessToProcTable(Process);			// (2) save process information 
	insertProcessToBlockQueue(Process);			// (3)  insert the process to BlockQueue
}

/**
* (3) test success !!
*	insert process to BlockQueue
*/
int insertProcessToBlockQueue(struct process* Process) {
	struct process* reference = &blockedQueue;	// pointing to LL Head
	if (reference == NULL) {
		printf("Initial BlockQueue's Head is NULL!! \n");
		exit(1);
	}
	Process->next = &blockedQueue;		//  NEW -> next 
	blockedQueue.prev = Process;			// head -> prev
	while (reference->next != &blockedQueue) {
		reference = reference->next;		// moving
	}
	reference->next = Process;					//  before -> next
	Process->prev = reference;				// NEW -> prev
	return ++(blockedQueue.len);
}

///////////////////////////////////////////////////////////////////////////////
///////////////// System Call : make IO Request Event /////////////////////////
///////////////////////////////////////////////////////////////////////////////
/**
* [Whole] test success !!
*	emit IO Request Event
*
*/
void makeIoEvent(struct process* Process, int requestCount) {
	struct ioDoneEvent* ioEvent = createIoDoneEventForQueue(Process, requestCount);		// (1) io event object create
	insertIoDoneEventToQueue(ioEvent);													// (2) in to the Queue
}

/*
* (1) test success !!
* make IoDoneEvent instance (for Queue  -  io doing)
*
* [[[[[[catioun]]]]]]] requestCount    ->     ioServTime must start at '0'
*/
struct ioDoneEvent* createIoDoneEventForQueue(struct process* Process, int requestCount) {
	struct ioDoneEvent* createdInstance = (struct ioDoneEvent*)malloc(sizeof(struct ioDoneEvent));
	createdInstance->procid = Process->id;
	createdInstance->doneTime = ioServTime[requestCount];
	createdInstance->len = (ioDoneEventQueue.len)+1;
	createdInstance->prev = NULL;
	createdInstance->next = NULL;
	return createdInstance;
}

/*
* (2) test success !!
* insert variable to the Queue (for Queue  -  io doing)
*/
void insertIoDoneEventToQueue(struct ioDoneEvent* createdIoDoneEvent) {
	struct ioDoneEvent* reference = &ioDoneEventQueue;	// pointing to LL Head
	if (reference == NULL) {
		printf("Initial referenceIoDoneEventQueue is NULL!! \n");
		exit(1);
	}
	createdIoDoneEvent->next = &ioDoneEventQueue;		//  NEW -> next 
	ioDoneEventQueue.prev = createdIoDoneEvent;			// head -> prev
	while (reference->next != &ioDoneEventQueue) {
		reference = reference->next;		// moving
	}
	reference->next = createdIoDoneEvent;					//  before -> next
	createdIoDoneEvent->prev = reference;				// NEW -> prev
	(ioDoneEventQueue.len)++;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////// Interruipt : IO Done interrupt  /////////////////////////
///////////////////////////////////////////////////////////////////////////////
// (n) numbering is logically FLOW !! 
/**
* Whole
* this is ISR  -  main Logic ( like configure() )
*
* [[[[[Caution]]]]] its not just 'A' Process - but handle to every IO Done process.
* [[[[[Caution]]]]] Include  -  process to terminate
**/
void interruptServiceRoutine(int* bitVector, int vectorSize, int currentTime) {
	int i = 0;
	for (i = 0; i < vectorSize; i++) {
		struct process* reference = &blockedQueue;
		if (bitVector[i] == -1) {
			break;
		}
		while (reference->next != &blockedQueue) {
			reference = reference->next;
			if (bitVector[i] == reference->id) {					
				reference = deleteNodeOfProcQueue(reference, &blockedQueue);				
				if (reference->serviceTime != reference->targetServiceTime) {
					moveProcessFromBlockToReady(reference);				
				}
				else {
					terminateProcess(reference, currentTime);		// i think this is no need
				}
				break;
			}
		}
	}
}

/**
* [test success !!]
* literally ...  move "process" from blockQ to readyQ
*/
void moveProcessFromBlockToReady(struct process* Process) {
	struct process* ref = &readyQueue;
	if (Process->serviceTime == Process->targetServiceTime) {
		printf("moveProcessFromBlockToReady() error - parameter process must not call this : to terminate\n");
		exit(1);
	}
	changeProcessState(Process, S_READY);		// (1) change the process's state field 
	saveProcessToProcTable(Process);			// (2) copy to PCB . ### ???ì¿????? ?ì¼??À§ ??????Æ®
	insertProcessToReadyQueue(Process);			// (3) now insert the process to readyQ
}


/**  (1)
* [test success !!]
*  (a) initialize the IO Done Arr 
* -> like complete Queue
* ########caution !###########  -> every cycle, must be handled & empty
*/
void initIoDoneEventArr(int NIOREQ) {
	int i = 0;
	for (i = 0; i < NIOREQ; i++) {
		ioDoneEvent[i].procid = -1;
		ioDoneEvent[i].doneTime = -1;
		ioDoneEvent[i].len = -1;
		ioDoneEvent[i].prev = NULL;
		ioDoneEvent[i].next = NULL;
	}
}

/**  (2)
* [test success !!]
*  (b) speculate What is IO Done
*    use "Bit Vector - co"
* bitvector start at "index 0"
*/
int* checkIoIsDone(int* bitVector, int ioQueSize) {
	int i = 0;
	struct ioDoneEvent* reference = &ioDoneEventQueue;
	if (reference == NULL) {
		printf("checkIoIsDone() - referrence is NULL error! \n");
		exit(1);
	}
	for (i = 0; i < ioQueSize; i++) {			// initiating is here
		bitVector[i] = -1;
	}
	i = 0;
	while (reference->next != &ioDoneEventQueue) {		// fill in the first... (??À¸?? ??Á¶?? Ã¹??Â°???? Ã¤????)
		reference = reference->next;
		if (reference->doneTime == 0) {
			bitVector[i++] = reference->procid;
		}
	}
	return bitVector;
}

/**  (3) 
* [test success !!]
* fill in the complete Array (IO Done Array) (done Q)
* using the (a) & (B)
*/
void emitInterrupt(int* bitVector, int ioQueSize) {
	int i = 0, j = 0;
	for (i = 0; i < ioQueSize; i++) {
		if (bitVector[i] != -1) {
			ioDoneEvent[j++].procid = bitVector[i];
		}
		else {
			break;
		}
	}
}

/** (4)
* [test success !!]
* delete IoDoneEvents of IoDoneEventQueue (doing Q)
*/
void deleteIoDoneEvent(int* bitVector, int vectorSize) {
	int i;
	for (i = 0; i < vectorSize; i++) {
		struct ioDoneEvent* reference = &ioDoneEventQueue;
		if (bitVector[i] == -1) {
			break;
		}
		while (reference->next != &ioDoneEventQueue) {	// [ caution ] when empty
			reference = reference->next;
			if (bitVector[i] == reference->procid) {
				struct ioDoneEvent* beDeleted = deleteNodeOfIoQueue(reference);
				free(beDeleted);
				break;
			}
		}
	}
}