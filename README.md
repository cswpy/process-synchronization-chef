# Project 4 Write-Up
@author: Phillip Wang pw1287

## Introduction
This is a program that simulates the collaborative working of four processes, one chef process that coordinates other processes and collect execution statistics, and four saladmaker processes that carries out work. You should first utilize the Makefile to compile the source code using `make clean && make`. First command will create a new txt file for logging. The second command compiles executables from the source code. The chef  process should be invoked using
```bash
./chef.o -n <number_of_salad> -m <chef_break_time> -t <saladmaker_work_time>
```
-t is an optional argument that will default to 5 seconds.

The chef process will invoke three saladmakers automatically using the following command
```bash
./saladmaker.o -m <saladmaker_break_time> -s <shared_memory_id>
```
P.S. All of the above parameters should be integers

## Semaphores
The four processes coordinate through semaphores and through variables in shared memory. All structures in shared memory are defined in *shared_memory_structure.hpp*. *log_file_sem* protects the exclusive right to write the *log.txt*. 

*shared_mutex* protects the variables in the shared memory so that only one process will be changing it. It protects the modification of *num_salad*, *num_workers_busy*, and *last_worker_start_time*. 

*ring_chef* let the chef wait for the saladmaker to take the materials before proceed to generating the new materials. This means that before notifying the chef, the saladmaker has exclusive access to the shared memory.

The three *saladmaker_\<id\>* semaphores are used to notifying the correct saladmaker when random materials are generated.