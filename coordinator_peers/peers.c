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
#include <sys/time.h>
#include <time.h>
#include <math.h>
#include <string.h>


union senum {int val;struct semidds *buff;unsigned short *array;};

struct entry{   //struct for entries
    int *numberofreadings;
    int *numberofwritings;
    char *entryp;
};

int main(int argc, char *argv[]){
    if (argc <= 4) {        //if users doesn't give enough arguments stop execution
        printf("You need to give more arguments\n");
        exit(0);
    }
    srand(time(NULL));

    int numofpeer = atoi(argv[1]);
    int numofentries = atoi(argv[2]);
    int loops= atoi(argv[3]);
    int readers=atoi(argv[4]);
    int writers=100-readers;
    key_t key;

    int write_segment_id;
    int read_segment_id;
    int segment_id[numofentries];
    int sem_id;
    int sem_fd;
    struct entry entries[numofentries];
    key_t wkey;
    wkey=123;
    key_t rkey;
    rkey=456;
    struct sembuf sop[numofentries];
    int *writingsnum= malloc(sizeof(int));
    int *readingsnum= malloc(sizeof(int));
    write_segment_id = shmget (wkey, sizeof(int), IPC_CREAT | 0666);    //shared memory for writings number
    writingsnum= (int*) shmat (write_segment_id, NULL, 0);
    read_segment_id = shmget (rkey, sizeof(int), IPC_CREAT | 0666);     //shared memory for readings number
    readingsnum= (int*) shmat (read_segment_id, NULL, 0);

    sem_fd = open("sem1.key", O_RDONLY);    //open sem1.key
    if (sem_fd < 0) {       //open failed
        perror("Could not open sem key for reading");
        exit(1);
    }
    if (read(sem_fd, &key, sizeof(key_t)) != sizeof(key_t)) {   //read key's value
        perror("Error reading the semaphore key");
        exit(2);
    }
    close(sem_fd);  //close file

    sleep(2);
    int readcnt=0;      //readings count
    int writecnt=0;     //writings count
    struct timeval start, end;      //for time
    long mtime, secs, usecs,totaltime;
    totaltime=0;

    int x=(readers*loops)/100;  //reader
    char *tmp;
    for(int j=0;j<loops;j++) {
        int r1 = rand()%2 ;     //choose whether peer reads or writes
        int r2 = rand() % numofentries;     //a random entry
        gettimeofday(&start, NULL);     //start for time

        if (r1 ==0) {
            segment_id[r2]=shmget (r2, sizeof(char), 0666 );    //get shared memory
            entries[r2].entryp=(char *) shmat(segment_id[r2],NULL,0);
            tmp=entries[r2].entryp;
            while(*tmp=='*'){   //if shared memory has a '*' it's read so don't write
                sleep(1);
            }
            sem_id = semget(key, numofentries, 0);  //get semaphore
            if (sem_id < 0) {   //semget failed
                perror("Could not create sem");
                exit(3);
            }

            sop[r2].sem_num = 0;    //update values of sop for r2 entry's semaphore to wait
            sop[r2].sem_op = -1;
            sop[r2].sem_flg = SEM_UNDO;

            double t=-log(rand()%2)/2;  //time for occupying entry

            gettimeofday(&end, NULL);
            secs  = end.tv_sec  - start.tv_sec;
            usecs = end.tv_usec - start.tv_usec;
            mtime = ((secs)  + usecs*10^9) + 0.5;   //ellapsed time
            totaltime+=mtime;
            //printf("Elapsed time: %ld millisecs\n", mtime);

            semop(sem_id, sop, 1);
            sleep(t*10);
            sop[r2].sem_op = 1;
            semop(sem_id, sop, 1);      //update semaphore since we finished with the entry
            writecnt++;                 //writings counter update

        }
        else{
            double t=-log(rand()%2)/2;  //time for occupying entry
            gettimeofday(&end, NULL);
            secs  = end.tv_sec  - start.tv_sec;
            usecs = end.tv_usec - start.tv_usec;
            mtime = ((secs) + usecs*10^9) + 0.5;   //ellapsed time
            totaltime+=mtime;
            //printf("Elapsed time: %ld millisecs\n", mtime);

            segment_id[r2]=shmget (r2, sizeof(char), 0666 );    //get shared memory for entry
            entries[r2].entryp=(char *) shmat(segment_id[r2],NULL,0);
            tmp=entries[r2].entryp;
            *tmp='*';   //while reading make it '*' so noone writes
            readcnt++;
            sleep(t*100);
            *tmp='0';   //update memory

        }
    }
    *readingsnum+=readcnt;  //add to overall readings
    *writingsnum+=writecnt; //add to overall writings
    printf("Time is:%ld, number of reads: %d, number of writes: %d\n", totaltime/((long) loops), readcnt, writecnt);   //readings and writings for current entry

    return 0;
}

