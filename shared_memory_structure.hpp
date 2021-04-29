#ifndef SHARED_MEMORY_STRUCT
#define SHARED_MEMORY_STRUCT

#include <semaphore.h>

struct shared_memory_struct{
    sem_t log_file_sem;
    sem_t shared_mutex;
    sem_t ring_chef;
    int saladmaker1_pid;
    int saladmaker2_pid;
    int saladmaker3_pid;
    sem_t saladmaker1;
    sem_t saladmaker2;
    sem_t saladmaker3;
    int weight_tomato;
    int weight_onion;
    int weight_pepper;
    int tomato_used[3];
    int onion_used[3];
    int pepper_used[3];
    int num_salad;
    int num_salad_makers[3];
    int num_worker_busy;
    double chef_start_time;
    double last_worker_start_time;
    double salad_making_time[3];
    double material_waiting_time[3];
};
#endif