/* This is a user input based test program for mp3. It asks user what operation to perform. It also asks user id and group id
    and sets them to the effective uid and gid of the current process through system calls setegid and seteuid
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <linux/unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>

void doeventopen_test(void);
void doeventclose_test(void);
void doeventwait_test(void);
void doeventsig_test(void);
int doeventinfo_test();
void doeventchown_test(void);
void doeventchmod_test(void);
void doeventstat_test(void); 
void setuidgid(void);
void signal_all(void);
uid_t uid;
gid_t gid;
int id, gidP;

void main() {
	int numP = 1;
	int i, nproc, num, index;
	while(1) {
		/* A menu presented to user. The user selects the option number*/
		printf("\n1: Create event, 2: Destroy event, 3: Wait, 4: Signal, 5: Event info, 6: Change owner,7: Change mode, 8: Event stat,9: Change currrent uid and gid, Any other number to exit\n");
		printf("Enter an option\n");
		scanf("%d", &num);
		// System calls called based on user input value. It exits on entering any number apart from 1 to 8 inclusive.
		switch(num) {
			case 1:
				doeventopen_test();
				break;
			case 2:
				doeventclose_test();
				break;
			case 3:
				doeventwait_test();
				break;
			case 4:
				doeventsig_test();
				break;
			case 5:
				doeventinfo_test();
				break;
			case 6:
				doeventchown_test();
				break;
			case 7:
				doeventchmod_test();
				break;
			case 8:
				doeventstat_test();
				break;
			case 9:
				setuidgid();
				break;
			default:
				signal_all();
				wait();
				exit(0);				
				break;
		}
	}
}

/* Takes uid and gid from user and sets them in to effective uid and gid of current process.
   This is done to check user access control in the system calls. This is called before any system call
   which may perform some write operation on the event list.
 */
void setuidgid() {
	seteuid(0);
	printf("Enter user id:\n");
	scanf("%d", &uid);
	printf("Enter group id:\n");
	scanf("%d", &gid);		
	setegid(gid);
	seteuid(uid);
}

/* Tests doeventopen system call.*/
void doeventopen_test() {
	int num, id, i;
	printf("Number of events to create:\n");
	scanf("%d", &num);
	printf("\nCreate %d events.\n", num);
	for (i=0;i<num;i++){
		id = syscall(181,  NULL);
	}
}

/* Tests doeventclose system call. Takes event id from user.*/
void doeventclose_test() {
	int event, res;
	printf("Event id to close:\n");
	scanf("%d", &event);
	res = syscall(182,  event);
	if(res == -1) {
		printf("Event destroy failed:%d:::",event);
	} else {
		printf("Event destroyed:%d::: Processes closed:%d",event, res);
	}
}

/* Tests doeventwait system call. Forks a child process which calls the system call to put itself on waiting queue.*/
void doeventwait_test() {
	int event, res;
	printf("Event id to wait:\n");
	scanf("%d", &event);
	if(fork() == 0) {
		res = syscall(183, event);
		if(res == -1) {
			printf("Event wait failed:%d:::",event);
		} else {
			printf("Event wait successful:%d %d",event, res);
		}
		printf("Child exits");
		exit(0);
	} else {
		printf("Parent continues\n");
	}
}

/* Tests doeventsig system call. Takes input event id from user and signals it to unblock all waiting processes*/
void doeventsig_test() {
	int event, res;
	printf("Event id to signal:\n");
	scanf("%d", &event);
	res = syscall(184,  event);
	if(res == -1) {
		printf("Event signal failed:%d:::",event);
	} else {
		printf("Event signaled:%d::: Processes closed:%d",event, res);
	}
}

/* Tests doeventinfo system call. Gets the event info of the events*/
int doeventinfo_test() {
	int num, isNotNull, *ids, i;	
	printf("Event number of events to get:");scanf("%d", &num); // Number of events ids to fill in the array.
	printf("\nIs pointer array null? 0 for yes:");scanf("%d", &isNotNull);// Pass null is parameter for the array
	if(isNotNull) {
		// Allocates the array size according to the num variable if array pointer is not to be passed as null
		ids = alloca (sizeof (int) * num);
		num = syscall (185, num, ids);
		if(num==-1){
			printf("Failure in doeventinfo().\n");
		} else{ 
			printf("Number of active events is: %d\n", num);
			printf("Print events : ");
			for (i = 0; i < num; i++) {
				printf ("%d ", ids[i]);
			}
			printf("\n");		
		}
	} else {
		num = syscall (185, 1, NULL);
		if(num == -1){
			printf("Failure in doeventinfo().\n");
		} else {
			printf("Number of active events is: %d\n", num);
		}
	}
	return num;
}

/* Tests doeventchown system call.Takes event id, UID and GID from user to set to the event*/
void doeventchown_test() {
	int event, res, uidVal, gidVal;
	uid_t uid;
	gid_t gid;
	printf("Event id to change owner:\n");scanf("%d", &event);
	printf("Uid change owner:\n");scanf("%d", &uidVal);
	printf("Gid to change group:\n");scanf("%d", &gidVal);
	uid = uidVal;
	gid = gidVal;
	res = syscall(205, event, uid, gid);
	if(res) {
		printf("Failure in doeventchown().\n");
	} else {
		printf("Owner changed: %d\n", res);
	}
}

/* Tests doeventchmod system call. Takes event id, UID flag and GID Flag from user to set to the event*/
void doeventchmod_test() {
	int event, res, uidVal, gidVal;
	printf("Event id to change owner:\n");scanf("%d", &event);
	printf("Uid flag:\n");scanf("%d", &uidVal);
	printf("Gid flag:\n");scanf("%d", &gidVal);
	res = syscall(211, event, uidVal, gidVal);
	if(res) {
		printf("Failure in doeventchmod().\n");
	} else {
		printf("Flags changed: %d\n", res);
	}
}

/* Tests doeventstat system call.*/
void doeventstat_test() {
	int event, res, *uidFlagptr = malloc(sizeof(int)), *gidFlagptr = malloc(sizeof(int));;
	uid_t *uidptr = malloc(sizeof(uid_t));
	gid_t *gidptr = malloc(sizeof(gid_t));
	printf("Event id to get stat:\n");scanf("%d", &event);
	res = syscall(214, event, uidptr, gidptr, uidFlagptr, gidFlagptr);
	if(res) {
		printf("Failure in doeventstat().\n");
	} else {
		printf("Event id, uid, gid, uidFlag and gidFlag are: %d %d %d %d %d.\n", event, *uidptr, *gidptr, *uidFlagptr, *gidFlagptr);
	}
}

/* Gets all the events and signals them. Used before exiting the program*/
void signal_all() {
	int num, *ids, i;
	num = syscall (185, -1, NULL);
	if(num == -1){
		printf("Failure in doeventinfo().\n");
	} else {
		ids = alloca (sizeof (int) * num);
		num = syscall (185, num, ids);
		if(num == -1){
			printf("Failure in doeventinfo().\n");
		}
		for (i = 0; i < num; i++) {
			syscall(184,  ids[i]);
		}	
	}
}

void printCurrentuidgid() {
	printf("Current uid and gid %d  %d:", (int)geteuid(), (int)getegid());
}
