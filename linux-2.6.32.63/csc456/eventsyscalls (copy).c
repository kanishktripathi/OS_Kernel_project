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

static int searchEvent(int eventID, struct event *event);
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
asmlinkage long sys_doeventclose(int eventID)
{ 
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

asmlinkage long sys_doeventwait(int eventID)
{ 
	if(eventID<0){		
		return -1;			/* immediately return if the id is invalid (negative event id) */
	}

	/* Declare variables */
	unsigned long flags;	/* for the lock */
	struct event *waitEvent = NULL;
	struct list_head *i;	/* pointer to the head of the allevents list */ 
	bool found = false;	/* boolean variable to show if an event with the requested id actually exists */

	
	read_lock_irqsave(&lock, flags);	/* save the current state of interrupts, disable interrupts and acquire lock for reading */
	
	/* Search the allevents list for the event with the requested id */
	list_for_each(i, &allevents.eventlist)
	{
		waitEvent=list_entry(i, struct event, eventlist);
		if(waitEvent->id == eventID){
			found = true;
			break;
		}		
	}
	read_unlock_irqrestore(&lock, flags);	
	if(found) {
		//printk("Current UID, GID, %d %d\n", current->cred->euid, current->cred->egid);
		if(getTaskuid(current) == 0 || getTaskuid(current) == waitEvent->uid ||
		getTaskgid(current) == waitEvent->gid) {
			//write_lock_irqsave(&lock, flags);
			waitEvent->condition = 0;
			wait_event_interruptible(waitEvent->queue, waitEvent->condition != 0);
			//write_unlock_irqrestore(&lock, flags);
			return 1;	
		} else {
			printk("User event not found");
			return -1;		
		}
	} else {
		return -1;	
	}
}

asmlinkage long sys_doeventsig(int eventID)
{ 
if(eventID<0){		
		return -1;			/* immediately return if the id is invalid (negative event id) */
	}

	/* Declare variables */
	unsigned long flags;	/* for the lock */
	struct event *waitEvent = NULL;
	struct list_head *i;	/* pointer to the head of the allevents list */ 
	bool found = false;	/* boolean variable to show if an event with the requested id actually exists */

	
	read_lock_irqsave(&lock, flags);	/* save the current state of interrupts, disable interrupts and acquire lock for reading */
	
	/* Search the allevents list for the event with the requested id */
	list_for_each(i, &allevents.eventlist)
	{
		waitEvent=list_entry(i, struct event, eventlist);
		//printk("current_event id is %d\n", waitEvent->id);
		if(waitEvent->id == eventID){
			found = true;
			break;
		}		
	}
	read_unlock_irqrestore(&lock, flags);	
	if(found) {
		if(getTaskuid(current) == 0 || getTaskuid(current) == waitEvent->uid ||
		getTaskgid(current) == waitEvent->gid) {
			waitEvent->condition = 0;
			int process = waitqueue_active(&waitEvent->queue);
			if(process) {
				struct list_head *j;
				process=0;
				list_for_each(j, &waitEvent->queue.task_list) {
					process++;
				}
				waitEvent->condition = 1;
				wake_up_all(&waitEvent->queue);
			}
			//printk("number of waiting processes is: %d\n",process);
			//write_unlock_irqrestore(&lock, flags);
			return process;	
		} else {
			return -1;		
		}
	} else {
		return -1;	
	}
}


/* 
 * sys_doeventinfo(): system call that fills the array of integers pointed to by eventIDs with the current set of active event IDs
 * input: num - number of integers which the memory pointed out by eventIDs can hold, int * eventIDs - the array mentioned before
 * returns: numbers of active ids on success, -1 on failure
 */
asmlinkage long sys_doeventinfo(int num, int * eventIDs)
{ 	
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
	
	read_lock_irqsave(&lock, flags);	/* save the current state of interrupts, disable interrupts and acquire lock for reading */
	
	/* Iterate through all the events list */
	list_for_each(i, &allevents.eventlist)
	{
		if (check < num && eventIDs != NULL){
			current_event=list_entry(i, struct event, eventlist);
			eventsarray[check]=current_event->id;
		}
		check++;	
	}
	
	read_unlock_irqrestore(&lock, flags);
	struct event *ev;
	int sEvent = searchEvent(1, ev);
	if(sEvent) {
		printk("Event found\n");
	} else {
		printk("Event not found\n");	
	}
	if (eventIDs != NULL){
		not_copied = copy_to_user(eventIDs, eventsarray,  sizeof(eventsarray));
		if (not_copied != 0) {
			printk(KERN_ALERT "ERROR: COPY TO USER\n"); //error in copy to user
			return -EFAULT;
		}
	}
	if(check < num) {
		check = -1;	
	}
	return check;
}

asmlinkage long sys_doeventchown(int eventID, uid_t UID, gid_t GID)
{ 
	

	return (0);
}


asmlinkage long sys_doeventchmod(int eventID, int UIDFlag, int GIDFlag)
{ 
	

	return (0);
}

asmlinkage long sys_doeventstat(int eventID, uid_t* UID, gid_t* GID, int* UIDFlag, int* GIDFlag)
{ 
	

	return (0);
}


/* -------------------------------   Utility Functions ------------------------------*/

static int searchEvent(int eventID, struct event *event) {
/* Declare variables */
	unsigned long flags;	/* for the lock */
	struct list_head *i;	/* pointer to the head of the allevents list */ 
	struct event *current_event = NULL;	/* pointer to the event that we are currently traversing */
	bool found = false;	/* boolean variable to show if an event with the requested id actually exists */

	printk("\nSearch event");
	read_lock_irqsave(&lock, flags);	// save the current state of interrupts, disable interrupts and acquire lock 							for reading 
	// Search the allevents list for the event with the requested id 
	list_for_each(i, &allevents.eventlist)
	{
		printk("in loop\n");
		current_event=list_entry(i, struct event, eventlist);
		printk("current_event id is %d\n", current_event->id);
		if(current_event->id == eventID){
			found = true;
			break;
		}		
	}
	read_unlock_irqrestore(&lock, flags);	
	printk("after unlock\n");
	if(found){
		event = current_event;
		return 1;			
	} else {
		return 0;
	}
}

static int getTaskuid(struct task_struct *task) {
	return task->cred->euid;
}

static int getTaskgid(struct task_struct *task) {
	return task->cred->egid;
}

