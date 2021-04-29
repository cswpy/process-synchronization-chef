#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/times.h>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include <fstream>
#include <cstring>
#include <sys/errno.h>
#include <sys/wait.h>
#include <iomanip>
#include <semaphore.h>

#define TOMATO_STD 80
#define ONION_STD 30
#define PEPPER_STD 50
#include "shared_memory_structure.hpp"

using namespace std;

int main(int argc, char *argv[]){
    int saladmaker_break_time = -1;
    int id = -1;
    cout.precision(8);

    for(int i=1; i<argc; i++){
        if(strcmp(argv[i], "-m") == 0){
            saladmaker_break_time = atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "-s") == 0){
            id = atoi(argv[++i]);
        }
    }

    if(id < 0 or saladmaker_break_time < 0){
        perror("[ERROR] Invalid arguments");
        exit(0);
    }

    struct shared_memory_struct *mem;
    mem = (struct shared_memory_struct *)shmat(id, (void *)0, 0);
    if(*(int *)mem == -1){
        perror("[ERROR] Saladmaker failed to attach memory.");
        exit(3);
    }

    fstream log_file;
    log_file.open("log.txt", ios_base::app);
    log_file << fixed << setprecision(2);

    // Determining which material the saladmaker is missing
    pid_t mypid = getpid();
    sem_wait(&mem->shared_mutex);
    int *material1, *material2, *salad_count;
    int material1_std, material2_std;
    int material3;
    sem_t *material_ready_sem;
    int *material1_sum, *material2_sum, *material3_sum;
    int saladmaker_id;
    double *salad_making_clock, *material_waiting_clock;
    double chef_start_time, currentTime, t1, t2;
    struct tms tb1, tb2, tb3;
    double ticspersec;
    double tmp;

    ticspersec = (double) sysconf(_SC_CLK_TCK);

    // Initializing semaphores and shared memory locations for access and modification
    if(mem->saladmaker1_pid == 0){ // Saladmaker1 has tomatoes
        mem->saladmaker1_pid = mypid;
        sem_post(&mem->shared_mutex);
        saladmaker_id = 1;
        material1 = &mem->weight_onion;
        material2 = &mem->weight_pepper;
        salad_count = &mem->num_salad_makers[0];
        material3 = TOMATO_STD;
        material1_std = ONION_STD;
        material2_std = PEPPER_STD;
        salad_making_clock = &mem->salad_making_time[0];
        material_waiting_clock = &mem->material_waiting_time[0];
        material1_sum = &mem->onion_used[0];
        material2_sum = &mem->pepper_used[0];
        material3_sum = &mem->tomato_used[0];
        material_ready_sem = &mem->saladmaker1;
    }
    else if(mem->saladmaker2_pid == 0){ // Saladmaker 2 has onions
        mem->saladmaker2_pid = mypid;
        sem_post(&mem->shared_mutex);
        saladmaker_id = 2;
        material1 = &mem->weight_tomato;
        material2 = &mem->weight_pepper;
        salad_count = &mem->num_salad_makers[1];
        material3 = ONION_STD;
        material1_std = TOMATO_STD;
        material2_std = PEPPER_STD;
        salad_making_clock = &mem->salad_making_time[1];
        material_waiting_clock = &mem->material_waiting_time[1];
        material1_sum = &mem->tomato_used[1];
        material2_sum = &mem->pepper_used[1];
        material3_sum = &mem->onion_used[1];
        material_ready_sem = &mem->saladmaker2;
    }
    else if(mem->saladmaker3_pid == 0){ // Saladmaker 3 has peppers
        mem->saladmaker3_pid = mypid;
        sem_post(&mem->shared_mutex);
        saladmaker_id = 3;
        material1 = &mem->weight_tomato;
        material2 = &mem->weight_onion;
        salad_count = &mem->num_salad_makers[2];
        material3 = PEPPER_STD;
        material1_std = TOMATO_STD;
        material2_std = ONION_STD;
        salad_making_clock = &mem->salad_making_time[2];
        material_waiting_clock = &mem->material_waiting_time[2];
        material1_sum = &mem->tomato_used[2];
        material2_sum = &mem->onion_used[2];
        material3_sum = &mem->pepper_used[2];
        material_ready_sem = &mem->saladmaker3;
    }
    else{
        sem_post(&mem->shared_mutex);
        perror("[ERROR] Exceeded three saladmakers.");
        exit(7);
    }
    chef_start_time = mem->chef_start_time;

    // Making the salad
    int material1_recv = 0, material2_recv = 0;
    do{
        // Start the clock for measuring waiting time
        t1 = (double) times(&tb1);
        sem_wait(&mem->log_file_sem);
        currentTime = (double) times(&tb3);
        tmp = (currentTime - chef_start_time) / ticspersec;
        log_file << "[" << tmp << "s][" << mypid << "][Saladmaker " << saladmaker_id << "] Waiting for ingredients" << endl;
        sem_post(&mem->log_file_sem);
        sem_wait(material_ready_sem);

        // Stop the clock and add the time to shared memory
        t2 = (double) times(&tb2);
        (*material_waiting_clock) += ((t2 - t1) / ticspersec);

        if(mem->num_salad <= 0){
            sem_post(&mem->ring_chef);
            break;
        }

        sem_wait(&mem->log_file_sem);
        currentTime = (double) times(&tb3);
        log_file << "[" << (currentTime - chef_start_time) / ticspersec << "s][" << mypid << "][Saladmaker " << saladmaker_id << "] Started working" << endl;
        sem_post(&mem->log_file_sem);
        
        // Start the clock for measuring working time
        t1 = (double) times(&tb1);

        cout << "[Saladmaker " << saladmaker_id << "] Received material 1 " << *material1 << endl;
        material1_recv += (*material1);
        *material1 = 0;

        cout << "[Saladmaker " << saladmaker_id << "] Received material 2 " << *material2 << endl;
        material2_recv += (*material2);
        *material2 = 0;

        sem_post(&mem->ring_chef);
        
        // If materials are insufficient, log it and return to waiting new materials
        if(material1_recv < material1_std || material2_recv < material2_std){
            cout << "[Saladmaker " << saladmaker_id << "] Materials inadequate, waiting for more materials" << endl;
            sem_wait(&mem->log_file_sem);
            currentTime = (double) times(&tb3);
            log_file << "[" << (currentTime - chef_start_time) / ticspersec << "s][" << mypid << "][Saladmaker " << saladmaker_id << "] Inadequate materials, waiting for chef" << endl;
            sem_post(&mem->log_file_sem);
            continue;
        }

        sem_wait(&mem->shared_mutex);
        (mem->num_worker_busy)++;
        cout << "Active workers: " << mem->num_worker_busy << endl;
        if(mem->num_worker_busy == 2){
            currentTime = (double) times(&tb3);
            mem->last_worker_start_time = (currentTime - chef_start_time) / ticspersec ;
        }
        sem_post(&mem->shared_mutex);
        
        (*material1_sum) += material1_recv;
        (*material2_sum) += material2_recv;
        material1_recv = 0;
        material2_recv = 0;        

        (*material3_sum) += material3;

        // Saladmaker takes a break for [0.8*salmkrtime, salmkrtime] seconds
        float coefficient = (rand() % 3 + 8) / (float)10;
        int sleeptime = saladmaker_break_time * coefficient * 1000000;
        cout << "[Saladmaker " << saladmaker_id << "] Working on salad for " << sleeptime << " ms" << endl;
        usleep(sleeptime);

        (*salad_count)++;

        sem_wait(&mem->shared_mutex);
        mem->num_salad--;
        sem_post(&mem->shared_mutex);
        
        // Stop the clock and add time to shared memory
        t2 = (double) times(&tb2);
        (*salad_making_clock) += ((t2 - t1) / ticspersec);

        sem_wait(&mem->log_file_sem);
        currentTime = (double) times(&tb3);
        log_file << "[" << (currentTime - chef_start_time) / ticspersec << "s][" << mypid << "][Saladmaker " << saladmaker_id << "] Finished salad" << endl;
        
        sem_wait(&mem->shared_mutex);
        (mem->num_worker_busy)--;
        cout << "Active workers: " << mem->num_worker_busy << endl;
        if(mem->num_worker_busy == 1){
            double previousWorkerStart = mem->last_worker_start_time;
            currentTime = (double) times(&tb3);
            log_file << "[INFO]Two or more workers are working from " << previousWorkerStart << "s -- " << (currentTime - chef_start_time) / ticspersec << "s\n";
        }
        sem_post(&mem->log_file_sem);
        sem_post(&mem->shared_mutex);
    } while(true);

    cout << "[Saladmaker " << saladmaker_id << "] Terminating" << endl;

    // Close the file and detach the shared memory
    log_file.close();
    shmdt(mem);
    return EXIT_SUCCESS;
}