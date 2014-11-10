#include <linux/kernel.h>
#include <linux/errno.h>
#include <asm/current.h>
#include <linux/uaccess.h>
#include <linux/list.h>
#include <linux/signal.h>
#include <linux/event.h>
#include <linux/spinlock.h>
#include <linux/linkage.h>
#include <linux/slab.h>
#include <linux/sched.h>


struct event allevents;	/* Have a global event struct that all processes can access */
rwlock_t lock; /* Use a reader-writer spin lock to protect the list of events since it's going to be both searched and updated */

static int validateEvent(int userCheck, struct event *found);
static int validateUserEvent(struct event *found);
static uid_t getTaskuid(struct task_struct *task);
static gid_t getTaskgid(struct task_struct *task);
static int isValidEventId(int eventId);

/* Initializing the events list and the wait queue. During the kernel boot time.*/
void __init doevent_init(void)
{
	lock = RW_LOCK_UNLOCKED;	/* Initialy no thread of execution holds the lock */
	INIT_LIST_HEAD(&allevents.eventlist);
	allevents.id = -1;		/* Initialize the eventid to -1 (invalid event id) */
	init_waitqueue_head(&allevents.queue);	/* Initialize the waiting queue that is going to hold all the events */
}


/* 
 * sys_doeventopen(): syscall 181. system call that creates a new event
 * returns: event ID on success or -1 on failure
 */
asmlinkage long sys_doeventopen()
{ 
	/* Declare variables */
	unsigned long flags;	/* for the lock */
	int lastid = -1;	/* variable holding the last event's id. Initially set to -1 (invalid event id) */
	
	/* Allocate space for a new event */	
	struct event* newevent = kmalloc(sizeof(struct event), GFP_KERNEL);
	if (newevent == NULL){
		return -1;
	}

	/* Statically initialize the event's list */
	INIT_LIST_HEAD(&(newevent->eventlist));  

	/* Add the event's list to the global list of all the events */
	write_lock_irqsave(&lock, flags);	/* disable interrupts for write access, otherwise a reader in an interrupt could deadlock 							   on the held write lock */
	list_add_tail(&(newevent->eventlist), &allevents.eventlist);
	
	/* Find the id of the last event in the list and compute the new event's id */
	lastid = list_entry((newevent->eventlist).prev, struct event, eventlist)->id;
	newevent->id = lastid + 1;
	newevent->uidFlag = 1;
	newevent->gidFlag = 1;
	current_euid_egid(&newevent->uid, &newevent->gid);
	init_waitqueue_head(&(newevent->queue));
	newevent->condition=0;
	write_unlock_irqrestore(&lock, flags);  /* release the lock and restore local interrupts to given previous state */	
	return newevent->id;
}

/* 
 * system call 182.system call that destroys an event given its id and signals any process waiting on the event to leave
 * params : event id
 * returns: number of processes signaled on success or -1 on failure
 */
asmlinkage long sys_doeventclose(int eventID) { 
	struct event *waitEvent = NULL;
	struct list_head *j, *i;	/* pointer to the head of the allevents list */ 
	unsigned long flags;	/* for the lock */
	int process = 0, retVal = 0;
	if(isValidEventId(eventID)) {
		retVal = -1;
	} else {
		read_lock_irqsave(&lock, flags);
		list_for_each(i, &allevents.eventlist) {
			waitEvent = list_entry(i, struct event, eventlist);
			if(waitEvent->id == eventID){
				break;
			} else {
				waitEvent = NULL;
			}		
		}
		if(validateEvent(1, waitEvent)) {
			read_unlock_irqrestore(&lock, flags);
			//Check if the queue has any waiting process.
			process = waitqueue_active(&waitEvent->queue);
			write_lock_irqsave(&lock, flags);
			if(process) {
				process = 0;
				//Increment process count for every process in waiting queue for the event
				list_for_each(j, &waitEvent->queue.task_list) {
					process++;
				}
				waitEvent->condition = 1;
				wake_up_all(&waitEvent->queue);
			}
			list_del_init(&waitEvent->eventlist);
			write_unlock_irqrestore(&lock, flags);
			kfree(waitEvent);
			retVal = process;
		} else {
			read_unlock_irqrestore(&lock, flags);
			retVal = -1;		
		}		
	}
	return retVal;
}

/* 
 * system call 183. system call that puts the current process issuing the system call to wait on the
 * event mentioned in the parameters
 * params : event id
 * returns: number of processes 1 success and -1 on failure
 */
asmlinkage long sys_doeventwait(int eventID) {
	struct event *waitEvent = NULL;
	struct list_head *i;	/* pointer to the head of the allevents list */ 
	unsigned long flags;	/* for the lock */
	int retVal = -1;
	if(isValidEventId(eventID)) {
		retVal = -1;
	} else {
		read_lock_irqsave(&lock, flags);
		list_for_each(i, &allevents.eventlist) {
			waitEvent = list_entry(i, struct event, eventlist);
			if(waitEvent->id == eventID){
				break;
			} else {
				waitEvent = NULL;
			}		
		}
		if(validateEvent(1, waitEvent)) {
			read_unlock_irqrestore(&lock, flags);
			//Put the event on waiting queue
			waitEvent->condition = 0;
			wait_event_interruptible(waitEvent->queue, waitEvent->condition != 0);
			retVal = 1;
		} else {
			read_unlock_irqrestore(&lock, flags);	
			retVal = -1;
		}
	}
	return retVal; 
}

/* 
 * system call 184.system call that signals any process waiting on the event to leave the event. Similar to
 * doeventclose but does not destroy the event.
 * params : event id
 * returns: number of processes signaled on success or -1 on failure
 */
asmlinkage long sys_doeventsig(int eventID) {
	struct event *waitEvent = NULL;
	struct list_head *i, *j;	/* pointer to the head of the allevents list */ 
	unsigned long flags;	/* for the lock */
	int retVal = -1, process = 0;
	if(isValidEventId(eventID)) {
		retVal = -1;
	} else {
		read_lock_irqsave(&lock, flags);
		list_for_each(i, &allevents.eventlist) {
			waitEvent = list_entry(i, struct event, eventlist);
			if(waitEvent->id == eventID){
				break;
			} else {
				waitEvent = NULL;
			}		
		}
		if(validateEvent(1, waitEvent)) {
			read_unlock_irqrestore(&lock, flags);
			process = waitqueue_active(&waitEvent->queue);
			if(process) {
				write_lock_irqsave(&lock, flags);
				process = 0;
				list_for_each(j, &waitEvent->queue.task_list) {
					process++;
				}
				waitEvent->condition = 1;
				write_unlock_irqrestore(&lock, flags);
				wake_up_all(&waitEvent->queue);		
			}
			retVal = process;
		} else {
			read_unlock_irqrestore(&lock, flags);
			retVal = -1;		
		}
	}
	return retVal;
}


/* 
 * syscall 185.system call that fills the array of integers pointed to by eventIDs with the current set of active event IDs.
 * Returns -1 if total number of events is more than specified in the num parameters
 * parameter: num - number of integers which the memory pointed out by eventIDs can hold, int * eventIDs - the array in which event ids are
 * copied
 * returns: numbers of active ids on success, -1 on failure
 */
asmlinkage long sys_doeventinfo(int num, int * eventIDs) { 	
	/* Declare variables */	
	int eventsarray[num];
	unsigned long flags;	/* for the lock */
	struct list_head *i;	/* pointer to the head of the allevents list */ 
	struct event *current_event;	/* pointer to the event that we are currently traversing */
	int p;
	int check=0;
	if(num < 0 && eventIDs != NULL) {
		return -1;
	}
	for(p=0;p<num;p++) {
		eventsarray[p] = -1;	
	}
	if(eventIDs != NULL && copy_from_user(&p, eventIDs, sizeof(int))) {// Checks if the pointer is valid	
		return -1;
	}
	read_lock_irqsave(&lock, flags);
	list_for_each(i, &allevents.eventlist) {
		if (check < num && eventIDs != NULL){
			current_event=list_entry(i, struct event, eventlist);
			eventsarray[check]=current_event->id;
		}
		check++;	
	}
	read_unlock_irqrestore(&lock, flags);

	if (eventIDs != NULL && check <= num){
		if (copy_to_user(eventIDs, eventsarray,  sizeof(eventsarray))) {
			return -1;
		}
	}
	if(check > num && eventIDs != NULL) {
		check = -1;	
	}
	return check;
}

/* 
 * syscall 205.system call that changes the event's UID and GID
 * input: eventID: the event's ID, UID: the desired UID, GID: the desired GID
 * returns: -1 on failure, 0 otherwise
 */
asmlinkage long sys_doeventchown(int eventID, uid_t UID, gid_t GID) { 
	struct event *waitEvent;
	struct list_head *i;	/* pointer to the head of the allevents list */ 
	unsigned long flags;	/* for the lock */
	int retVal = -1;
	if(isValidEventId(eventID) || UID < 0 || GID < 0) {
		retVal = -1;
	} else {
		read_lock_irqsave(&lock, flags);
		list_for_each(i, &allevents.eventlist) {
			waitEvent = list_entry(i, struct event, eventlist);
			if(waitEvent->id == eventID){
				break;
			} else {
				waitEvent = NULL;		
			}		
		}
		if(validateUserEvent(waitEvent)) {
			read_unlock_irqrestore(&lock, flags);
			write_lock_irqsave(&lock, flags); //Obtain write lock to change event information
			waitEvent->uid = UID;
			waitEvent->gid = GID;
			write_unlock_irqrestore(&lock, flags);	
			retVal = 0;		
		} else {
			read_unlock_irqrestore(&lock, flags);	
		}
	}
	return retVal; 
}

/* 
 * syscall 211. system call that changes the event's UIDFlag and GIDFlag
 * input: eventID: the event's ID, UIDFlag: the desired User Signal Enable bit, GID: the desired Group Signal Enabled bit
 * returns: -1 on failure, 0 otherwise
 */
asmlinkage long sys_doeventchmod(int eventID, int UIDFlag, int GIDFlag) { 
	struct event *waitEvent;
	struct list_head *i;	/* pointer to the head of the allevents list */ 
	unsigned long flags;	/* for the lock */
	int retVal = -1;
	if(isValidEventId(eventID)) {
		retVal = -1;
	} else {
		read_lock_irqsave(&lock, flags);
		list_for_each(i, &allevents.eventlist) {
			waitEvent = list_entry(i, struct event, eventlist);
			if(waitEvent->id == eventID){
				break;
			} else {
				waitEvent = NULL;		
			}
		}		
		if(validateUserEvent(waitEvent)) {
			read_unlock_irqrestore(&lock, flags);
			write_lock_irqsave(&lock, flags);
			waitEvent->uidFlag = UIDFlag;
			waitEvent->gidFlag = GIDFlag;
			write_unlock_irqrestore(&lock, flags);	
			retVal = 0;		
		} else{
			read_unlock_irqrestore(&lock, flags);
			retVal = -1;
		}
	}
	return retVal;
}

/* 
 * syscall 214. system call that places the  event's uid, gid, uidflag and gidflag in memory pointed by the user.
 * Does not places any info if a NULL value is passed in pointer parameters
 * input: eventID: the event's ID, UID: uid pointer, gid pointer , UIDFlag: user signal enable bit pointer,  GIDFlag: group signal 
 * enable pointer
 * returns: -1 on failure, 0 otherwise
 */
asmlinkage long sys_doeventstat(int eventID, uid_t* UID, gid_t* GID, int* UIDFlag, int* GIDFlag) { 
	uid_t thisUID;
	gid_t thisGID;
	int thisUIDFlag, thisGIDFlag;
	struct event *waitEvent;
	struct list_head *i;	/* pointer to the head of the allevents list */ 
	unsigned long flags;	/* for the lock */
	int retVal = 0;
	unsigned long not_copied1 = 0;
	unsigned long not_copied2 = 0;
	unsigned long not_copied3 = 0;
	unsigned long not_copied4 = 0;
	/* Check pointer validity for every pointer parameter.*/
	if( UID != NULL) {
		not_copied1 = copy_from_user(&thisUID, UID, sizeof(uid_t));
	}
	if( GID != NULL) {
		not_copied2 = copy_from_user(&thisGID, GID, sizeof(gid_t));
	}
	if( UIDFlag != NULL) {
		not_copied3 = copy_from_user(&thisUIDFlag, UIDFlag, sizeof(int));
	}
	if( GIDFlag != NULL) {
		not_copied4 = copy_from_user(&thisGIDFlag, GIDFlag, sizeof(int));
	}

	if (not_copied1 || not_copied2 || not_copied3 || not_copied4) {
		/*return if there's even a single copy failure*/
		return -1;
	}

	if(isValidEventId(eventID)) {
		retVal = -1;
	} else {
		read_lock_irqsave(&lock, flags);
		list_for_each(i, &allevents.eventlist) {
			waitEvent = list_entry(i, struct event, eventlist);
			if(waitEvent->id == eventID){
				thisUID = (int)waitEvent->uid;
				thisGID = (int)waitEvent->gid;
				thisUIDFlag = waitEvent->uidFlag;
				thisGIDFlag = waitEvent->gidFlag;
				break;
			} else {
				waitEvent = NULL;		
			}		
		}
		read_unlock_irqrestore(&lock, flags);
		if(waitEvent == NULL) {
			return -1;
		}
		/* Copy all values to user space*/	
		if( UID != NULL) {
			not_copied1 = copy_to_user(UID, &thisUID, sizeof(uid_t));
		}
		if( GID != NULL) {
			not_copied2 = copy_to_user(GID, &thisGID, sizeof(gid_t));
		}
		if( UIDFlag != NULL) {		
			not_copied3 = copy_to_user(UIDFlag, &thisUIDFlag, sizeof(int));
		}
		if( GIDFlag != NULL) {		
			not_copied4 = copy_to_user(GIDFlag, &thisGIDFlag, sizeof(int));
		}
		if (not_copied1 || not_copied2 || not_copied3 || not_copied4 ) { // Check for copy error
			return -EFAULT;
		}
	}
	return retVal;
}


/* --------------------------------------- Utility Functions ------------------------------------------*/

/* 
 * validateEvent(): It checks whether the event is not null and looks for permission control to wait or signal an event.
 * input: userCheck: Check if access controls need to be evaluated, struct *event: event pointer to validate
 * returns: 0 on failure, 1 otherwise
 */
static int validateEvent(int userCheck, struct event *event) {
	/* Declare variables */
	int retVal = 0;
	if(event == NULL) {
		retVal = 0;
	} else {
		retVal = 1;		
		if(userCheck) {
			if(getTaskuid(current) == 0 || (getTaskuid(current) == event->uid && event->uidFlag == 1) ||
				(getTaskgid(current) == event->gid && event->gidFlag == 1)) {
				retVal = 1;		
			} else {
				retVal = 0;			
			}	
		}
	}
	return retVal;
}

/* 
 * validateUserEvent: Checks whether user id equals. current user id
 * input: struct *event: event pointer to validate
 * returns: 0 on failure, 1 otherwise
 */
static int validateUserEvent(struct event *event) {
	/* Declare variables */
	int retVal = 0;	/* boolean variable to show if an event with the requested id actually exists */
	// Search the allevents list for the event with the requested id 
	if(event == NULL) {
		retVal = 0;
	} else {
		retVal = 1;		
		if(getTaskuid(current) == 0 || getTaskuid(current) == event->uid) {
			retVal = 1;		
		} else {
			retVal = 0;			
		}
	}
	return retVal;
}

/* 
 * isValidEventId: Checks whether its a valid event id or not. Any positive number are acceptable.
 * input: struct *event: event pointer to validate
 * returns: 0 on failure, 1 otherwise
 */
static int isValidEventId(int eventId) {
	return eventId < 0;
}

/* 
 * getTaskuid: gets the effective user id for the task.
 * params: struct task_struct
 * returns: the effective uid of the task
 */
static uid_t getTaskuid(struct task_struct *task) {
	return task->cred->euid;
}

/* 
 * getTaskuid: gets the effective group id for the task.
 * params: struct task_struct
 * returns: the effective gid of the task
 */
static gid_t getTaskgid(struct task_struct *task) {
	return task->cred->egid;
}

/* -----------------------------------------------------------------------------------------------------*/

