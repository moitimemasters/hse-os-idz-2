#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include "common.h"

sem_t *semaphore, *initialization_sem;

int *all_flower_states;
int number_of_flowers = 10;

void interrupt_handler(int signum) {
    munmap(all_flower_states, sizeof(int) * number_of_flowers);
    sem_close(semaphore);
    sem_unlink(SEMAPHORE_NAME);
    sem_close(initialization_sem);
    sem_unlink("initialization_sem");
    exit(EXIT_SUCCESS);
}

int ensure_usage(int argc, char *argv[]) {
    if (argc != 5 || strcmp(argv[1], "-n") != 0 || strcmp(argv[3], "-t")) {
        fprintf(stderr,
                "Usage: %s -n <gardener_number> -t <number_of_flowers>\n",
                argv[0]);
        exit(EXIT_FAILURE);
    }
    number_of_flowers = atoi(argv[4]);
    if (number_of_flowers <= 0) {
        fprintf(stderr, "Number of flowers must be positive\n");
        exit(EXIT_FAILURE);
    }
    return atoi(argv[2]);
}

int main(int argc, char *argv[]) {
    int gardener_number = ensure_usage(argc, argv);
    signal(SIGINT, interrupt_handler);
    initialization_sem = sem_open("initialization_sem", 0);
    sem_wait(initialization_sem);
    int shared_mem_fd = shm_open(SHARED_MEM_NAME, O_RDWR, S_IRWXU);
    all_flower_states =
        mmap(NULL, sizeof(int) * number_of_flowers, PROT_READ | PROT_WRITE,
             MAP_SHARED, shared_mem_fd, 0);

    semaphore = sem_open(SEMAPHORE_NAME, 0);
    sem_post(initialization_sem);
    srand(time(NULL));
    while (true) {
        int count_dead = 0;
        for (int i = 0; i < number_of_flowers; ++i) {
            sem_wait(semaphore);
            if (all_flower_states[i] == -1) {
                all_flower_states[i] += 2;
                printf("Садовник #%d поливает цветок #%d\n", gardener_number,
                       i);
            }
            if (all_flower_states[i] < -1 || all_flower_states[i] > 1) {
                count_dead++;
            }
            sem_post(semaphore);
            usleep((rand() % 500) * 100 + 1000);
        }
        if (count_dead == number_of_flowers) {
            printf("Садовник #%d уходит домой\n", gardener_number);
            break;
        }
    }
    interrupt_handler(-1);
}
