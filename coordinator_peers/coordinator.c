#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <math.h>
#include <string.h>


struct entry{               //struct for entry
    int *numberofreadings;
    int *numberofwritings;
    char *entryp;
};



int main(int argc, char *argv[]) {

    if (argc <= 4) {            //if users doesn't give enough arguments stop execution
        printf("You need to give more arguments\n");
        exit(0);
    }
    srand(time(NULL));

    int pid;
    int sem_id;
    int status;
    int numofpeer = atoi(argv[2]);      //number of peers
    int numofentries = atoi(argv[3]);   //number of entries
    int loops= atoi(argv[4]);           //number of loops
    struct sembuf sop[numofentries];    //buffer for semaphores
    int sem_fd;                         //semaphore file
    key_t key;
    key_t wkey;                         //key for writings shared memory
    wkey=123;
    key_t rkey;                         //key for readings shared memory
    rkey=456;

    int seg_id[numofentries];           //stores id returned by shmget
    int *w,*r;
    struct entry entries[numofentries];     //array of entries
    int *writingsnum;
    int *readingsnum;
    sem_id = shmget (wkey, sizeof(int), IPC_CREAT | 0666);  //shared memory for writings number
    writingsnum= (int*) shmat (sem_id, NULL, 0);
    w=writingsnum;
    *w=0;
    sem_id = shmget (rkey, sizeof(int), IPC_CREAT | 0666);  //shared memory for readings number
    readingsnum= (int*) shmat (sem_id, NULL, 0);
    r=readingsnum;
    *r=0;
    char *tmp;

    for (int i = 0; i <numofentries ; ++i) {                    //shared memory for entries
        seg_id[i] = shmget (i, sizeof(char), IPC_CREAT|0666 );
        entries[i].entryp= (char*) shmat (seg_id[i], 0, 0);
        tmp=entries[i].entryp;
        *tmp='0';
    }

    key=ftok("./coordinator.c",66);     //get key

    sem_fd = open("sem1.key", O_WRONLY | O_TRUNC | O_EXCL | O_CREAT, 0644); //key in sem1.key file

    if (sem_fd < 0) {       //open failed
        perror("Could not open sem.key");
        exit(1);
    }

    if (write(sem_fd, &key, sizeof(key_t)) < 0) {   //write value in key
        perror("Could not write key to file");
        exit(2);
    }
    close(sem_fd);  //close file
    sem_id = semget(key, numofentries, IPC_CREAT | IPC_EXCL | 0600);    //semaphores for entries
    if (sem_id < 0) {   //semget failed
        perror("Could not create sem");
        unlink("sem1.key");
        exit(3);
    }
    if (semctl(sem_id, 0, SETVAL, 0) < 0) {     //initialize semaphores
        perror("Could not set value of semaphore");
        exit(4);
    }

    for(int i = 0; i < numofpeer; i++) {    //make child processes
        pid = fork();
        if(pid < 0) {   //fork failed
            printf("Error\n");
            exit(1);
        } else if (pid == 0) {  //child procces
            printf("Child (%d): %d\n", i + 1, getpid());
            if(execvp(argv[1],&argv[1])==-1){   //execute peers
                printf("ERROR in execvp\n");
                exit(4);
            }
            exit(0);
        }
        sleep(2);
    }

    sleep(5);
    for(int i=0;i<numofentries;i++) {   //semaphores array values
        sop[i].sem_num = i;
        sop[i].sem_op = 1;
        sop[i].sem_flg = 0;

    }
    if (semop(sem_id, sop, numofentries)) {     //increment semaphores
        perror("Could not increment semaphore");
        exit(5);
    }

    for (;;) {      //wait for child processes to finish
        pid = wait(&status);
        if (pid < 0) {
            if (errno == ECHILD) {
                printf("All children have exited\n");
                break;
            }
            else {
                perror("Could not wait");
            }
        }
        else {
            printf("Child %d exited with status %d\n", pid, status);
        }
    }

    if (semctl(sem_id, 0, IPC_RMID) < 0) {  //delete semaphores
        perror("Could not delete semaphore");
    }

    printf("Readings are %d and writings are %d\n", *readingsnum, *writingsnum);    //print total numbers of writings and readings

    return 0;
}