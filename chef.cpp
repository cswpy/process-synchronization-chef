#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include <fstream>
#include <cstring>
#include <iomanip>
#include <sys/times.h>
#include <sys/errno.h>
#include <sys/wait.h>
#include <semaphore.h>

#include "shared_memory_structure.hpp"

using namespace std;

int main(int argc, char *argv[]){
    // Call chef in the form ./chef.o -n 10 -m 5 -t 3
    int number_of_salad = -1;
    int chef_break_time = -1;
    int saladmaker_break_time = 5;

    cout.precision(8);

    for(int i=1; i<argc; i++){
        if(strcmp(argv[i], "-n") == 0){
            number_of_salad = atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "-m") == 0){
            chef_break_time = atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "-t") == 0){
            saladmaker_break_time = atoi(argv[++i]);
        }
    }

    if(number_of_salad <= 0 or chef_break_time < 0 or saladmaker_break_time < 0){
        perror("[ERROR] Invalid arguments");
        exit(0);
    }
    
    double chef_start_time, ticspersec;
    struct tms tb1, tb2;
    chef_start_time = (double) times(&tb1);
    double currentTime;
    
    ticspersec = (double) sysconf(_SC_CLK_TCK);

    // Creating shared memory and initialize shared memory variables and semaphores
    int id;
    id = shmget(IPC_PRIVATE, sizeof(shared_memory_struct), 0666);
    if(id == -1){
        perror("[ERROR] Chef failed to create shared memory in chef.");
        exit(2);
    }
    cout << "[INFO] Created shared memory w/ id " << id << endl;
    struct shared_memory_struct *mem;
    mem = (struct shared_memory_struct *)shmat(id, (void *)0, 0);
    if(*(int *)mem == -1){
        perror("[ERROR] Chef failed to attach memory.");
        exit(3);
    }
    cout << "[INFO] Chef attached shared memory." << endl;
    mem->weight_onion = 0;
    mem->weight_pepper = 0;
    mem->weight_tomato = 0;
    mem->num_salad = number_of_salad;
    mem->num_worker_busy = 0;
    mem->saladmaker1_pid = 0;
    mem->saladmaker2_pid = 0;
    mem->saladmaker3_pid = 0;
    mem->chef_start_time = chef_start_time;
    mem->last_worker_start_time = (double) 0;
    
    for(auto p=mem->pepper_used; *p; p++)
        *p = 0;
    for(auto p=mem->tomato_used; *p; p++)
        *p = 0;
    for(auto p=mem->onion_used; *p; p++)
        *p = 0;
    for(auto p=mem->num_salad_makers; *p; p++)
        *p = 0;
    for(auto p=mem->salad_making_time; *p; p++)
        *p = 0.0;
    for(auto p=mem->material_waiting_time; *p; p++)
        *p = 0.0;    
    
    sem_init(&mem->log_file_sem, 1, 1);
    sem_init(&mem->saladmaker1, 1, 0);
    sem_init(&mem->saladmaker2, 1, 0);
    sem_init(&mem->saladmaker3, 1, 0);
    sem_init(&mem->shared_mutex, 1, 1);
    sem_init(&mem->ring_chef, 1, 0);

    fstream log_file;
    sem_wait(&mem->log_file_sem);
    log_file.open("log.txt", ios_base::app);
    log_file << fixed << setprecision(2);
    currentTime = (double) times(&tb2);
    log_file << "======New Execution======" << endl;
    log_file << "[" << (currentTime - chef_start_time) / ticspersec << "s] Chef finished initialization." << endl;
    sem_post(&mem->log_file_sem);

    // Spawning three saladmakers
    int saladmakerpid;
    for(int i=0; i<3; i++){
        if((saladmakerpid=fork()) == -1){
            perror("[ERROR] Chef failed to fork.");
            exit(4);
        }
        else if(saladmakerpid == 0){
            char const *shm_id = to_string(id).c_str();
            char const *break_time = to_string(saladmaker_break_time).c_str();
            execl("./saladmaker.o", "saladmaker.o", "-m", break_time, "-s", shm_id, (char *)NULL);
            perror("[ERROR] Chef failed to spawn saladmaker");
            exit(5);
        }
    }
    
    // Preparing two random materials
    srand(time(0));
    
    while(mem->num_salad > 0){
        int saladmaker_num = rand() % 3 + 1;
        float tomato_weight, onion_weight, pepper_weight;

        sem_wait(&mem->log_file_sem);
        currentTime = (double) times(&tb2);
        log_file << "[" << (currentTime - chef_start_time) / ticspersec << "s][" << getpid() << "][Chef] Generating materails for saladmaker" << saladmaker_num << endl;
        sem_post(&mem->log_file_sem);

        if(saladmaker_num == 1){
            // Generates onions and peppers
            onion_weight = rand() % 13 + 24; // [24, 36]
            pepper_weight = rand() % 21 + 40; // [40, 60]
            
            mem->weight_onion = onion_weight;
            mem->weight_pepper = pepper_weight;

            sem_post(&mem->saladmaker1);
        }
        else if(saladmaker_num == 2){
            // Generates tomatoes and peppers
            tomato_weight = rand() % 33 + 64; // [64, 96]
            pepper_weight = rand() % 21 + 40; // [40, 60]
            
            mem->weight_tomato = tomato_weight;
            mem->weight_pepper = pepper_weight;
            
            sem_post(&mem->saladmaker2);
        }
        else if(saladmaker_num == 3){
            // Generates onions and tomatoes
            tomato_weight = rand() % 33 + 64; // [64, 96]
            onion_weight = rand() % 13 + 24; // [24, 36]

            mem->weight_tomato = tomato_weight;
            mem->weight_onion = onion_weight;

            sem_post(&mem->saladmaker3);
        }
        // Wait for saladmaker to take the ingredients
        sem_wait(&mem->ring_chef);

        // Chef takes a break for [0.5*break_time, break_time] seconds
        float coefficient = (rand() % 6 + 5) / (float)10;
        int break_time = chef_break_time * coefficient * 1000000;
        cout << "[Chef] Taking a break for " << break_time << " ms\n";
        usleep(break_time);

    }
    cout << "[Chef] Terminating all saladmakers" << endl;
    sem_post(&mem->saladmaker1);
    sem_post(&mem->saladmaker2);
    sem_post(&mem->saladmaker3);
    
    int status;
    int wpid;
    while((wpid = wait(&status)) > 0);
    currentTime = (double) times(&tb2);
    log_file << "[" << (currentTime - chef_start_time) / ticspersec << "s] Chef finished execution." << endl;
    log_file.close();

    // Print statistics
    for(int i=0; i<3; i++){
        cout << "======Saladmaker" << (i+1) << "======" << endl;
        cout << "Number of salads:\t" << mem->num_salad_makers[i] << endl << "Tomato:\t" << mem->tomato_used[i] << "g\n" << "Onion:\t" << mem->onion_used[i] << "g\n" << "Pepper:\t" << mem->pepper_used[i] << "g\n" ;
        cout << "Total waiting time:\t" << mem->material_waiting_time[i] << "s\n" << "Total working time:\t" << mem->salad_making_time[i] << "s\n" << "=======================\n";
    }

    // Dispose semaphores and shared memory properly
    sem_destroy(&mem->log_file_sem);
    sem_destroy(&mem->saladmaker1);
    sem_destroy(&mem->saladmaker2);
    sem_destroy(&mem->shared_mutex);
    sem_destroy(&mem->ring_chef);
    if(shmdt(mem) == -1){
        perror("[ERROR] Chef failed to detach shared memory.");
        exit(5);
    }
    if(shmctl(id, IPC_RMID, NULL) == -1){
        perror("[ERROR] Chef failed to destroy shared memory.");
        exit(6);
    }
    return EXIT_SUCCESS;
}
