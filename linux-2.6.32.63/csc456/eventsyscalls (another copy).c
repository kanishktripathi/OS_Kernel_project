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

static struct event *searchAndReturnEvent(int eventID, int userCheck, int *found);
static int getTaskuid(struct task_struct *task);
static int getTaskgid(struct task_struct *task);

void __init doevent_init(void)
{
	lock = RW_LOCK_UNLOCKED;	/* Initialy no thread of execution holds the lock */
	INIT_LIST_HEAD(&allevents.eventlist);
	allevents.id = -1;		/* Initialize the eventid to -1 (invalid event id) */
	init_waitqueue_head(&allevents.queue);	/* Initialize the waiting queue that is going to hold all the events */
}


/* 
 * sys_doeventopen(): system call that creates a new event
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
 * sys_doeventclose(): system call that destroys an event given its id and signals any process waiting on the event to leave the event
 * returns: number of processes signaled on success or -1 on failure
 */
asmlinkage long sys_doeventclose(int eventID) { 
	/*struct event *waitEvent;
	struct list_head *j;	
	unsigned long flags;	
	int  found = 0, process = 0, retVal = 0;
	read_lock_irqsave(&lock, flags);
	waitEvent = searchAndReturnEvent(eventID, 1, &found);
	read_unlock_irqrestore(&lock, flags);
	if(found) {
		printk("Found event: %d", waitEvent->id);
		process = waitqueue_active(&waitEvent->queue);
		write_lock_irqsave(&lock, flags);
		if(process) {
			list_for_each(j, &waitEvent->queue.task_list) {
				process++;
			}
			waitEvent->condition = 1;
			wake_up_all(&waitEvent->queue);
		}
		list_del(&(waitEvent->eventlist));
		write_unlock_irqrestore(&lock, flags);
		kfree(waitEvent);
		retVal = process;
	} else {
		retVal = -1;
	}
	return retVal;*/
	if(eventID<0){		
		return -1;			/* immediately return if the id is invalid (negative event id) */
	}

	/* Declare variables */
	unsigned long flags;	/* for the lock */
	struct list_head *i;	/* pointer to the head of the allevents list */ 
	struct event *current_event = NULL;	/* pointer to the event that we are currently traversing */
	bool found = false;	/* boolean variable to show if an event with the requested id actually exists */

	
	read_lock_irqsave(&lock, flags);	/* save the current state of interrupts, disable interrupts and acquire lock for reading */
	
	/* Search the allevents list for the event with the requested id */
	list_for_each(i, &allevents.eventlist)
	{
		current_event=list_entry(i, struct event, eventlist);
		if(current_event->id == eventID){
			found = true;
			break;
		}		
	}
	
	read_unlock_irqrestore(&lock, flags);	/*  */
	if(!found){
		printk("no event found\n");
		return -1;
	}else{

		/* If the event was found, delete if from the event's list */
		write_lock_irqsave(&lock, flags);
		list_del(&(current_event->eventlist));
		write_unlock_irqrestore(&lock, flags);
		
		/* Free the space previously held by this event */
		kfree(current_event);
	}

	return (0);    //needs change
}

asmlinkage long sys_doeventwait(int eventID) {
	struct event *waitEvent;
	unsigned long flags;
	int found = 0, retVal = 0;
	read_lock_irqsave(&lock, flags);
	waitEvent = searchAndReturnEvent(eventID, 1, &found);
	read_unlock_irqrestore(&lock, flags);
	if(found) {
		waitEvent->condition = 0;
		wait_event_interruptible(waitEvent->queue, waitEvent->condition != 0);
		retVal = 1;
	} else {
		retVal = -1;
	}
	return retVal; 
}

asmlinkage long sys_doeventsig(int eventID) {
	struct event *waitEvent = NULL;
	struct list_head *j;	/* pointer to the head of the allevents list */ 
	unsigned long flags;
	int found = 0, process = 0, retVal = 0;
	read_lock_irqsave(&lock, flags);
	waitEvent = searchAndReturnEvent(eventID, 1, &found);
	read_unlock_irqrestore(&lock, flags);
	if(found) {
		process = waitqueue_active(&waitEvent->queue);
		if(process) {
			list_for_each(j, &waitEvent->queue.task_list) {
				process++;
			}
			waitEvent->condition = 1;
			wake_up_all(&waitEvent->queue);		
		}
		retVal = process;
	} else {
		retVal = -1;
	}
	return retVal;
}


/* 
 * sys_doeventinfo(): system call that fills the array of integers pointed to by eventIDs with the current set of active event IDs
 * input: num - number of integers which the memory pointed out by eventIDs can hold, int * eventIDs - the array mentioned before
 * returns: numbers of active ids on success, -1 on failure
 */
asmlinkage long sys_doeventinfo(int num, int * eventIDs) { 	
	//int *ptr;
	if(num < 0) {
		return -1;
	}
	int eventsarray[num];
	int p;
	for(p=0;p<num;p++) {
		eventsarray[p] = 0;	
	}
	unsigned long not_copied = -1;
	/* Declare variables */
	unsigned long flags;	/* for the lock */
	struct list_head *i;	/* pointer to the head of the allevents list */ 
	struct event *current_event;	/* pointer to the event that we are currently traversing */
	int check=0;
	
	read_lock_irqsave(&lock, flags);
	list_for_each(i, &allevents.eventlist) {
		if (check < num && eventIDs != NULL){
			current_event=list_entry(i, struct event, eventlist);
			eventsarray[check]=current_event->id;
		}
		check++;	
	}
	read_unlock_irqrestore(&lock, flags);
	/*struct event *e = NULL;
	read_lock_irqsave(&lock, flags);
	int flag = searchAndReturnEvent(1, 1, e);
	read_unlock_irqrestore(&lock, flags);
	if(flag) {
		printk("Event found\n");	
	} else {
		printk("Event not found\n");	
	}*/
	/*if(num < check) {
		check = -1;	
	}
	if (eventIDs != NULL){
		not_copied = copy_to_user(eventIDs, eventsarray,  sizeof(eventsarray));
		if (not_copied != 0) {
			printk(KERN_ALERT "ERROR: COPY TO USER\n"); //error in copy to user
			return -EFAULT;
		}
	}*/
	return check;
}

asmlinkage long sys_doeventchown(int eventID, uid_t UID, gid_t GID) { 
	unsigned long flags;	/* for the lock */
	int  found = 0, retVal = 0;
	struct event *waitEvent;
	read_lock_irqsave(&lock, flags);
	waitEvent = searchAndReturnEvent(eventID, 1, &found);
	read_unlock_irqrestore(&lock, flags);
	if(UID < 0 || GID < 0) {
		found = 0;	
	}
	if(found) {
		write_lock_irqsave(&lock, flags);
		waitEvent->uid = UID;
		waitEvent->gid = GID;
		write_unlock_irqrestore(&lock, flags);	
		retVal = 0;
	} else {
		retVal = -1;
	}
	return retVal; 
}


asmlinkage long sys_doeventchmod(int eventID, int UIDFlag, int GIDFlag) { 
	unsigned long flags;	/* for the lock */
	int  found = 0, retVal = 0;
	struct event *waitEvent;
	read_lock_irqsave(&lock, flags);
	waitEvent = searchAndReturnEvent(eventID, 1, &found);
	read_unlock_irqrestore(&lock, flags);
	if(waitEvent != NULL) {
		write_lock_irqsave(&lock, flags);
		waitEvent->uidFlag = UIDFlag;
		waitEvent->uidFlag = GIDFlag;
		write_unlock_irqrestore(&lock, flags);	
		retVal = 0;
	} else {
		retVal = -1;
	}
	return retVal;
}

asmlinkage long sys_doeventstat(int eventID, uid_t* UID, gid_t* GID, int* UIDFlag, int* GIDFlag)
{ 
	

	return (0);
}


/* -------------------------------   Utility Functions ------------------------------*/

static struct event *searchAndReturnEvent(int eventID, int userCheck, int *flag) {
/* Declare variables */
	struct list_head *i;	/* pointer to the head of the allevents list */ 
	struct event *current_event = NULL;	/* pointer to the event that we are currently traversing */
	int found = 0;	/* boolean variable to show if an event with the requested id actually exists */
	// Search the allevents list for the event with the requested id 
	if(eventID < 0) {
		*flag = 0;
		return current_event;	
	}
	list_for_each(i, &allevents.eventlist) {
		current_event=list_entry(i, struct event, eventlist);
		if(current_event->id == eventID){
			*flag = 1;
			break;
		}		
	}
	if(found) {
		if(userCheck) {
			if(getTaskuid(current) == 0 || (getTaskuid(current) == current_event->uid && current_event->uidFlag == 1) ||
				(getTaskgid(current) == current_event->gid && current_event->gidFlag == 1)) {
				*flag = 1;		
			} else {
				*flag = 0;			
			}	
		}
	} else {
		*flag = 0;
	}
	return current_event;
}

static int getTaskuid(struct task_struct *task) {
	return task->cred->euid;
}

static int getTaskgid(struct task_struct *task) {
	return task->cred->egid;
}

