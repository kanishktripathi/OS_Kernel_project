#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <linux/unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>

int test_eventbasic(void);
int test_eventwaitsig(void);
int test_eventchown(void);
int test_eventchmod(void);
int test_accesswaitsig(void);
int test_accessflags(void);
int test_notasks(void);

int main(int argc, char *argv[]){

	//----Perform some unit tests: remove the comment to specify which one----//
	//test_eventbasic();
	//test_eventwaitsig();
	//test_eventchown();
	//test_eventchmod();
	//test_accesswaitsig();
	test_accessflags();
	//test_notasks();

	return 0;
}



/*--------------------------------------------Test Functions---------------------------------------------------*/
/* Function: test_eventbasic
 * Input: void
 * Output: 0 if successful, non-zero if non successful
 * Explanation: unit test for testing doeventopen(), doeventclose() and doeventinfo() work correctly    
 */
int test_eventbasic(void){
	int i, id, nproc, num, index;
	printf("\n--------running test_eventinfo---------\n");
	printf("\nCreate five events.\n");
	for (i=0;i<5;i++){
		id = syscall(181,  NULL);
	}

	printf("\nDelete event with eventID=2.\n");
	nproc = syscall(182, 2);
	if(nproc==-1){
		printf("Failure in deleting the event.\n");
		return -1;
	}else{
		printf("Number of processes signaled is: %d.\n", nproc);
		
	}	

	printf("\nPrint event info.\n");
	/* Get the set of events */
	num = syscall (185, 7, NULL);
	if(num==-1){
		printf("Failure in doeventinfo().\n");
		return -1;
	}else{
		printf("Number of active events is: %d\n", num);
	}
	int * ids = alloca (sizeof (int) * num);
	num = syscall (185, num, ids);
	if(num==-1){
		printf("Failure in doeventinfo().\n");
		return -1;
	}else{
		/* Print out the IDs */
		for (index = 0; index < num; ++index) {
			printf ("%d\n", ids[index]);
		} 
	}

	/* Check for num (second argument) < number of active events */
	printf("\nCall doeventinfo() for num<number of active ids.\n");
	num = syscall (185, 0, ids);
	if(num==-1){
		printf("Failure in doeventinfo().\n");
		return -1;
	}

	return 0;
}

/* Function: test_eventwaitsig()
 * Input: void
 * Output: 0 if successful, non-zero if non successful
 * Explanation: unit test for testing if doeventwait() and doeventsig() work correctly
 */
int test_eventwaitsig(void){
	int id;	
	int numP = -1;	
	printf("\n--------running test_evenwaitsig---------\n");
	printf("\nCreate an event.\n");
	id = syscall(181,  NULL);

	printf("\nMake two processes wait on the event and then signal them.\n");
	if(fork() == 0) {
		if(fork() == 0) {
			syscall(183,id);	
			printf("Child2 awakes!\n");
		} else {
			syscall(183,id);
			printf("Child1 awakes!\n");		
		} 
	} else {
		sleep(5);	
		numP = syscall(182,id);
		printf("Number of process signaled is:%d.\n",numP);
	}
	return 0;
}

/* Function: test_eventchown()
 * Input: void
 * Output: 0 if successful, non-zero if non successful
 * Explanation: unit test for testing if test_eventchown works correctly using doeventstat
 */
int test_eventchown(void){
	int id, success, checkstat;
	//int uid, gid, uidFlag, gidFlag;
	uid_t *uidptr = malloc(sizeof(uid_t));
	gid_t *gidptr = malloc(sizeof(gid_t));
	int *uidFlagptr = malloc(sizeof(int));
	int *gidFlagptr = malloc(sizeof(int));

	printf("\n--------running test_evenchown---------\n");
	printf("\nCreate an event.\n");
	id = syscall(181,  NULL);
	printf("\nChange the event's uid to to 10 and the gid to 15.\n");
	success = syscall(205, id, 10, 15);
	if(success==-1){
		printf("Failure in changing uid and gid\n");	
	}else{
		checkstat=syscall(214, id, uidptr, gidptr, uidFlagptr, gidFlagptr);
		printf("Event id, uid, gid, uidFlag and gidFlag are: %d %d %d %d %d.\n", id, *uidptr, *gidptr, *uidFlagptr, *gidFlagptr);	
	}

	return 0;
}


/* Function: test_eventchmod()
 * Input: void
 * Output: 0 if successful, non-zero if non successful
 * Explanation: unit test for testing if doeventchmod() works correctly using doeventstat    
 */
int test_eventchmod(void){
	int id, success, checkstat;
	//int uid, gid, uidFlag, gidFlag;
	uid_t *uidptr = malloc(sizeof(uid_t));
	gid_t *gidptr = malloc(sizeof(gid_t));
	int *uidFlagptr = malloc(sizeof(int));
	int *gidFlagptr = malloc(sizeof(int));

	printf("\n--------running test_evenchmod---------\n");
	printf("\nCreate an event.\n");
	id = syscall(181,  NULL);
	
	printf("\nCheck the current state of the event.\n");
	checkstat=syscall(214, id, uidptr, gidptr, uidFlagptr, gidFlagptr);
	printf("Event id, uid, gid, uidFlag and gidFlag are: %d %d %d %d %d.\n", id, *uidptr, *gidptr, *uidFlagptr, *gidFlagptr);
	
	printf("\nChange the event's uidflag to 0 and the gidFlag to 0.\n");
	success = syscall(211, id, 0, 0);
	if(success==-1){
		printf("Failure in changing uidFlag and gidFlag\n");	
	}else{
		checkstat=syscall(214, id, uidptr, gidptr, uidFlagptr, gidFlagptr);
		printf("Event id, uid, gid, uidFlag and gidFlag are: %d %d %d %d %d.\n", id, *uidptr, *gidptr, *uidFlagptr, *gidFlagptr);	
	}

	printf("\nNow change the event's uidflag to 1 and keep the gidFlag to 0.\n");
	success = syscall(211, id, 1, 0);
	if(success==-1){
		printf("Failure in changing uidFlag and gidFlag\n");	
	}else{
		checkstat=syscall(214, id, uidptr, gidptr, uidFlagptr, gidFlagptr);	
		printf("Event id, uid, gid, uidFlag and gidFlag are: %d %d %d %d %d.\n", id, *uidptr, *gidptr, *uidFlagptr, *gidFlagptr);
	}

	printf("\nNow try to change the event's uidflag to 2 and the gidFlag to 1.\n");
	success = syscall(211, id, 2, 1);
	if(success==-1){
		printf("Failure in changing uidFlag and gidFlag\n");	
	}else{
		checkstat=syscall(214, id, *uidptr, *gidptr, *uidFlagptr, *gidFlagptr);	
		printf("Event id, uid, gid, uidFlag and gidFlag are: %d %d %d %d %d.\n", id, *uidptr, *gidptr, *uidFlagptr, *gidFlagptr);
	}
	return 0;
}

/* Function: test_accesswaitsig()
 * Input: void
 * Output: 0 if successful, non-zero if non successful
 * Explanation: unit test for testing if event access controls for waiting and signaling work correctly. Specifically we're testing control number 5.
 */
int test_accesswaitsig(void){
	int id, success, retVal;	
	int numP = -1;
	retVal = 0;	
	printf("\n--------running test_accesscontrols---------\n");
	printf("\nCreate an event.\n");
	id = syscall(181,  NULL);
	
	printf("\nChange the event's euid to 11 and the egid to 20.\n");
	success = syscall(205, id, 11, 20);
	if(success==-1){
		printf("Failure in changing uid and gid\n");
		retVal = -1;	
	}

	printf("\nChange the process's euid to 5 and the egid to 20.\n");
	setegid(20);
	seteuid(5);

	printf("\nMake the child process wait on the event and then signal it.\n");
	if(fork() == 0) {
		syscall(183,id);
		printf("Child awakes!\n");	
		exit(1);	
	} else {
		sleep(5);	
		numP = syscall(182,id);
		printf("Number of process signaled is:%d.\n",numP);
	}

	printf("\nChange the event's gidFlag to 0.\n");
	success = syscall(211, id, 1, 0);

	printf("\nTry waiting and signaling after the gidFlag change.\n");
	if(fork() == 0) {
		success = syscall(183,id);
		if(success==-1){
			printf("Failure in waiting on the event.\n");
			retVal= -1;	
		}else{
			printf("Child awakes!\n");
		}		
	} else {
		sleep(5);	
		numP = syscall(182,id);
		if(numP==-1){
			printf("Failure in signaling.\n");
			retVal= -1;
		}else{
			printf("Number of process signaled is:%d.\n",numP);
		}
	}
	return retVal;
}

/* Function: test_accessflags()
 * Input: void
 * Output: 0 if successful, non-zero if non successful
 * Explanation: unit test for testing if event access controls UIDFlag and GIDFlag work correctly. Specifically we're testing control numbers 7 and 8.
 */
int test_accessflags(void){
	int id, success, retVal, checkstat;	
		uid_t *uidptr = malloc(sizeof(uid_t));
	gid_t *gidptr = malloc(sizeof(gid_t));
	int *uidFlagptr = malloc(sizeof(int));
	int *gidFlagptr = malloc(sizeof(int));
	int numP = -1;
	retVal = 0;	
	printf("\n--------running test_accesscontrols---------\n");
	printf("\nCreate an event.\n");
	id = syscall(181,  NULL);
	
	printf("\nChange the event's euid to 5 and the egid to 20.\n");
	success = syscall(205, id, 5, 20);
	if(success==-1){
		printf("Failure in changing uid and gid\n");
		retVal = -1;	
	}
	
	printf("\nChange the event's uidFlag and gidFlag to 0.\n");
	success = syscall(211, id, 0, 0);

	checkstat=syscall(214, id, uidptr, gidptr, uidFlagptr, gidFlagptr);	
	printf("Event id, uid, gid, uidFlag and gidFlag are: %d %d %d %d %d.\n", id, *uidptr, *gidptr, *uidFlagptr, *gidFlagptr);

	printf("\nChange the process's euid to 5.\n");
	seteuid(5);
	printf("\nChange the event's euid to 7.\n");
	success = syscall(205, id, 7, 20);
	if(success==-1){
		printf("Failure in changing uid and gid\n");
		retVal = -1;	
	}
	printf("\nChange the event's euid to 9.\n");
	success = syscall(205, id, 9, 20);
	if(success==-1){
		printf("Failure in changing uid and gid\n");
		retVal = -1;	
	}
	checkstat=syscall(214, id, uidptr, gidptr, uidFlagptr, gidFlagptr);	
	printf("Event id, uid, gid, uidFlag and gidFlag are: %d %d %d %d %d.\n", id, *uidptr, *gidptr, *uidFlagptr, *gidFlagptr);

	return retVal;
}


/* Function: test_notasks()
 * Input: void
 * Output: 0 if successful, non-zero if non successful
 * Explanation: unit test for testing signaling when no tasks are waiting
 */
int test_notasks(void){
	int id, retVal;	
	int numP = -1;
	retVal = 0;	
	printf("\n--------running test_program---------\n");
	printf("\nCreate an event.\n");
	id = syscall(181,  NULL);

	printf("\nSignal when no tasks are waiting\n");
	numP = syscall(182,id);
	printf("Number of process signaled is:%d.\n",numP);

	return retVal;
}


