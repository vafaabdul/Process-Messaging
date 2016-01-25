#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <semaphore.h>
#include <errno.h>
#include <signal.h>
#include <sys/shm.h>



#define TEXT_SZ 4096

struct shared_use_st
{

	int pid;
	int noOfProcess;
	int read;
	int dataWritten;
	char text[TEXT_SZ];

};

void help(char * appName){

	printf("Usage : %s\n",appName);
	printf("type exit to terminate the program\n" );
	return ;
}


void writeToShared(void * args,sem_t * sem_id){

	struct shared_use_st * shared_stuff=(struct shared_use_st *)args;


	char* in_line=NULL;
  	size_t read=0;
  	size_t capacity=0;

	do{

		read=getline(&in_line,&capacity,stdin);

	  	if(strcmp(in_line,"exit\n")==0){

	  			break;
	  	}

	  	if(read !=-1){

  		while(shared_stuff->dataWritten==1){

  			sleep(1);
  		}

  		sem_wait(sem_id);

  		shared_stuff->pid=getpid();
  		strncpy(shared_stuff->text,in_line,TEXT_SZ);
  		shared_stuff->dataWritten=1;
  		sem_post(sem_id);

  	}

  	}while(1);
}


void readFromShared(void * args,sem_t * sem_id){

	struct shared_use_st * shared_stuff=(struct shared_use_st *)args;

	pid_t localRead=0;

	while(1){

		if(shared_stuff->dataWritten ==1 && shared_stuff->pid != getppid() && localRead != shared_stuff->pid){

			sem_wait(sem_id);
			printf("Process %d sent : %s",(int)shared_stuff->pid,shared_stuff->text );
			shared_stuff->read++;
			if(shared_stuff->read==shared_stuff->noOfProcess-1){
				shared_stuff->dataWritten=0;
				shared_stuff->read=0;
			}

			localRead=shared_stuff->pid;
			sem_post(sem_id);

		}

		if(shared_stuff->dataWritten==0){
			localRead=0;
		}

	}
	
}

int main(int argc,char* argv[]){

	if(argc>1){

		if(strcmp(argv[1],"-h")==0){

			help(argv[0]);
		}

		exit(EXIT_SUCCESS);
	}

  	struct shared_use_st *shared_stuff;
  	void *shared_memory = (void *)0;

    int shmid=0;
    sem_t * sem_id;
    pid_t child;


    sem_id=sem_open("mysemaph", O_CREAT, 0600, 1);

    if(sem_id == SEM_FAILED) {
        perror("parent sem_open");
        return;
    }

    shmid = shmget( (key_t)1234,sizeof(struct shared_use_st),0666 |IPC_CREAT );


    if(shmid==-1){

    	perror("shmget: shmget failed"); 
    	exit(EXIT_FAILURE);
    }

    shared_memory = shmat(shmid,(void *)0, 0);


    if (shared_memory == (void *)-1) {

		perror("shmat failed");
		exit(EXIT_FAILURE);
	}

	shared_stuff = (struct shared_use_st *)shared_memory;
	
    sem_wait(sem_id);
    shared_stuff->noOfProcess++;
    sem_post(sem_id);

    if((child=fork())==-1){

		perror("fork error");
		exit(EXIT_FAILURE);

	}
	else if(child==0){

		readFromShared(shared_stuff,sem_id);
		
	}
	else{

			writeToShared(shared_stuff,sem_id);
			kill(child,SIGKILL);

	}

	sem_wait(sem_id);
	shared_stuff->noOfProcess--;
	int num_process=shared_stuff->noOfProcess;
	sem_post(sem_id);

	if (shmdt(shared_memory) == -1)
	{
			perror("shmdt failed");
			exit(EXIT_FAILURE);
	}


	if(num_process==0){

		if (shmctl(shmid, IPC_RMID, 0) == -1)
		{
			perror("shmctl error");
			exit(EXIT_FAILURE);
		}
	}
	
	exit(EXIT_SUCCESS);

}