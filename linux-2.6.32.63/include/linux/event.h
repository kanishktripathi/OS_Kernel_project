#include <linux/types.h>
#include <linux/wait.h>
#include <linux/list.h>


struct event
{
	int id;	
	uid_t uid;
	gid_t gid;
	int uidFlag;
	int gidFlag;
	int condition;	
	struct list_head eventlist;
	wait_queue_head_t queue;
	
};




