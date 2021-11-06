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
#include <string.h>
#include <sys/time.h>

union semnum {
    int value;
    struct semid_ds *buffer;
    unsigned int short *array;
};

int mtx, write_mtx, file_sem;   //first is for the processes to wait for feeder to write in shared memory

//write_mtx is for feeder to wait al processes to read from shared memory
//file_sem is to coordinate writing in result.txt

void P(int semid, int num) {    //downs a semphore
    struct sembuf p;
    p.sem_num = num;
    p.sem_op = -1;
    p.sem_flg = 0;
    if (semop(semid, &p, 1) == -1) {
        perror("P");
        exit(1);
    }
}

void V(int semid, int num) {    //ups a semaphore
    struct sembuf v;
    v.sem_num = num;
    v.sem_op = 1;
    v.sem_flg = 0;
    if (semop(semid, &v, 1) == -1) {
        perror("V");
        exit(1);
    }
}


int main(int argc, char *argv[]) {
    if (argc != 3) {        //check number of arguments
        printf("Invalid number of arguments\n");
        return -1;
    }
    pid_t pid = getpid();
    int shmid;
    int semid;
    int status;
    key_t SHMKEY, SEMKEY;
    SHMKEY = 15;        //for shared memory
    SEMKEY = 16;        //for mtx
    int SHMSIZE = sizeof(int) + sizeof(long);
    char *shmem;

    union semnum arg;
    key_t read_semkey = 17;     //for write_mtx
    key_t file_semkey = 18;     //for file_sem
    int read_semid;
    int proc_num = atoi(argv[1]);       //get number of processes
    int sizeM = atoi(argv[2]);          //get the size of the array
    int *array = malloc(sizeM * sizeof(int));
    pid_t proc[proc_num];

    union semnum u;
    u.value = 0;

    srand(time(NULL));
    for (int i = 0; i < sizeM; i++) {          //fill array with random number less than 100(this can change)
        array[i] = rand() % 100;
    }

    FILE *fptr;
    fptr = fopen("result.txt", "wa");       //file to write results

    if ((shmid = shmget(SHMKEY, SHMSIZE, 0600 | IPC_CREAT)) < 0) {      //create shared memory
        perror("shmget");
        exit(1);
    }

    if ((mtx = semget(SEMKEY, proc_num, 0666 | IPC_CREAT | S_IRUSR | S_IWUSR)) < 0) {       //create semaphore
        perror("semget");
        exit(1);
    }
    if ((write_mtx = semget(read_semkey, 1, 0666 | IPC_CREAT | S_IRUSR | S_IWUSR)) < 0) {   //create semaphore
        perror("semget read");
        exit(1);
    }
    if ((file_sem = semget(file_semkey, 1, 0666 | IPC_CREAT | S_IRUSR | S_IWUSR)) < 0) {    //create semaphore
        perror("semget");
        exit(1);
    }
    arg.value = 1;
    for (int k = 0; k < proc_num; ++k) {    //initialize semaphores
        if (semctl(mtx, k, SETVAL, arg) < 0) {
            perror("semctl");
            exit(1);
        }
    }
    if (semctl(file_sem, 0, SETVAL, arg) < 0) {
        perror("semctl");
        exit(1);
    }
    arg.value = proc_num;       //we initialize write_mtx with the number of processes
    if (semctl(write_mtx, 0, SETVAL, arg) < 0) {
        perror("semctl");
        exit(1);
    }

    if ((shmem = shmat(shmid, (char *) 0, 0)) == (char *) -1) {     //get shared memory
        perror("shmem");
        exit(1);
    }
    char curWorkDir[128];
    if (getcwd(curWorkDir, sizeof(curWorkDir)) == NULL) {
        perror("getcwd");
        exit(3);
    }
    char fullpath[256];
    sprintf(fullpath, "%s/Child", curWorkDir);

    printf("Printing array from parent:\n");
    for (int k = 0; k < sizeM; ++k) {
        printf("%d  ", array[k]);
    }
    for (int l = 0; l < proc_num; ++l) {        //first we make mtx down so all processes have to wait for feeder to start them

        P(mtx, l);
    }

    struct sembuf p;
    p.sem_num = 0;
    p.sem_op = -proc_num;
    p.sem_flg = 0;
    if (semop(write_mtx, &p, 1) == -1) {    //down write_mtx so that feeder will have to wait for processes to read before continuing to write
        perror("P");
        exit(1);
    }
    printf("\n\n");
    int k = 0;
    for (int i = 0; i < proc_num; i++) {    //create the processes
        //printf("%d\n", i);
        if ((pid = fork()) < 0) {
            perror("fork");
            exit(2);
        }
        if (pid == 0) {
            k = i;
            break;
        }

        if (pid > 0) {
            proc[i] = pid;
        }
    }


    if (pid > 0) {      //feeder-parent process


        struct timeval tv;
        for (int i = 0; i < sizeM; i++) {       //for each element in the arraay
            gettimeofday(&tv, NULL);
            long timestamp = ((tv.tv_sec) + tv.tv_usec * 10 ^ 9) + 0.5;     //we get current timestamp
            memmove(shmem, &array[i], sizeof(int));     //move i-th element of the array in shared memory
            memmove(shmem + sizeof(int), &timestamp, sizeof(long));     //move timestamp in shared memory

            struct sembuf v[proc_num];
            for (int l = 0; l < proc_num; ++l) {
                v[l].sem_num = l;
                v[l].sem_op = 1;
                v[l].sem_flg = 0;

            }
            if (semop(mtx, v, proc_num) == -1) {        //we up the semaphore set to start all processes
                perror("V");
                exit(1);
            }

            struct sembuf p;
            p.sem_num = 0;
            p.sem_op = -proc_num;
            p.sem_flg = 0;
            if (semop(write_mtx, &p, 1) == -1) {        //down write_mtx
                perror("P");
                exit(1);
            }
        }

        while (1) {     //wait for children to finish
            int status;
            pid_t finished = wait(&status);
            if (finished == -1) {
                if (errno == ECHILD)
                    break;
            } else {
                if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
                    printf("Process with pid %d failed\n", finished);
                    //exit(1);
                }
            }
        }
        shmdt(shmem);
        if (semctl(mtx, proc_num, IPC_RMID) < 0) {  //delete semaphores
            perror("Could not delete semaphore");
        }
        if (semctl(write_mtx, 0, IPC_RMID) < 0) {  //delete semaphores
            perror("Could not delete semaphore");
        }
        if (semctl(file_sem, 0, IPC_RMID) < 0) {  //delete semaphores
            perror("Could not delete semaphore");
        }
        free(array);
        fclose(fptr);
    } else if (pid == 0) {      //child
        long   mo = 0;
        int retval;
        struct timeval end;
        int *array2 = malloc(sizeM * sizeof(int));

        for (int j = 0; j < sizeM; ++j) {
            P(mtx, k);      //try to get the semaphore

            long timestamp = 0;
            memmove(&array2[j], shmem, sizeof(int));    //read from shared memory
            memmove(&timestamp, shmem + sizeof(int), sizeof(long));
            gettimeofday(&end, NULL);
            long curtime = ((end.tv_sec) + end.tv_usec * 10 ^ 9) + 0.5;     //get current timestamp
            int diff = curtime - timestamp;     //compare it with the one from the feeder
            mo += diff;
            V(write_mtx, 0);    //up write_mtx so that feeder knows that you are finished
        }
        char strid[10];
        sprintf(strid, "%d", getpid()); //write process id in result.txt
        P(file_sem, 0);     //down file sem so that only one process writes in the file at a time
        fputs(strid, fptr);
        fputs(":\n", fptr);
        for (int i = 0; i < sizeM; i++) {   //write the array in result.txt
            char strarr[10];
            sprintf(strarr, "%d  ", array2[i]);
            fputs(strarr, fptr);
        }
        fputs("\n\n", fptr);
        V(file_sem, 0);     //up to let them know you are finished
        free(array2);
        printf("Average for child %d is %ld\n", getpid(), mo);
    }

    return 0;
}
