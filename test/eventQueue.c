#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <linux/unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>

int main() {
	int i, id, nproc, num, index, *ptr, *ids;
	printf("\n--------running test_eventinfo---------\n");
	printf("\nCreate five events.\n");
	for (i=0;i<5;i++){
		setgid(i+1);			
		setuid(i);
		id = syscall(181,  NULL);
		setuid(0);
	}
	if(fork() == 0) {
		if(fork() == 0) {
			syscall(183,1);	
			printf("Child2 awakes!\n");
		} else {
			syscall(183,1);
			printf("Child1 awakes!\n");		
		} 
	} else {
		printf("Waiting for child\n");
		wait();
		printf("Parent exits\n");
		/*
		num = syscall (185, 7, NULL);
		if(num==-1){
			printf("Failure in doeventinfo().\n");
			return -1;
		}else{
			printf("Number of active events is: %d\n", num);
			ids = alloca (sizeof (int) * num);
			num = syscall (185, 20, ids);
		}
		if(num==-1){
			printf("Failure in doeventinfo().\n");
			return -1;
		}else{
			for (index = 0; index < num; ++index) {
				printf ("%d\n", ids[index]);
			}
			ptr = (int *)0xa012;
			num = syscall (185, 7, ptr); 
		}
		if(num==-1){ 
			printf("Failure in doeventinfo().\n");	
		}*/
	}
	
	
}
