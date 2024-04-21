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

sem_t *semaphore;
int *flower_state;

void interrupt_handler(int signum) {
    sem_close(semaphore);
    munmap(flower_state, sizeof(int));
    exit(EXIT_SUCCESS);
}

typedef struct input_args_t {
    int flower_number;
    int number_of_flowers;
} input_args_t;

input_args_t ensure_usage(int argc, char *argv[]) {
    if (argc != 5 || strcmp(argv[1], "-n") != 0 || strcmp(argv[3], "-t") != 0) {
        fprintf(stderr, "Usage: %s -n <flower_number> -t <number_of_flowers>\n",
                argv[0]);
        exit(EXIT_FAILURE);
    }
    return (input_args_t){atoi(argv[2]), atoi(argv[4])};
}

int main(int argc, char *argv[]) {
    input_args_t args = ensure_usage(argc, argv);
    signal(SIGINT, interrupt_handler);

    sem_t *initialization_sem = sem_open("initialization_sem", 0);
    if (initialization_sem == SEM_FAILED) {
        perror("sem_open/initialization_sem");
        exit(EXIT_FAILURE);
    }
    sem_wait(initialization_sem);
    int shared_mem_fd = shm_open(SHARED_MEM_NAME, O_RDWR, S_IRWXU);
    if (shared_mem_fd == -1) {
        perror("shm_open/shared_mem_fd");
        exit(EXIT_FAILURE);
    }
    sem_post(initialization_sem);

    semaphore = sem_open(SEMAPHORE_NAME, 0);

    if (semaphore == SEM_FAILED) {
        perror("sem_open/semaphore");
        exit(EXIT_FAILURE);
    }

    int *all_flower_states =
        (int *)mmap(NULL, sizeof(int) * args.number_of_flowers,
                    PROT_READ | PROT_WRITE, MAP_SHARED, shared_mem_fd, 0);
    flower_state = all_flower_states + args.flower_number;

    srand(time(NULL));
    while (true) {
        sem_wait(semaphore);
        if (*flower_state == 0) {
            if (rand() % 2 == 0) {
                *flower_state = -1;
                printf("Цветок #%d начинает вять.\n", args.flower_number);
            }
        } else if (*flower_state == 1) {
            *flower_state = 0;
            printf("Цветок #%d полит.\n", args.flower_number);
        } else if (*flower_state == -1) {
            (*flower_state)--;
            printf("Цветок #%d умер.\n", args.flower_number);
            sem_post(semaphore);
            break;
        } else if (*flower_state > 1) {
            printf("Цветок #%d перелит.\n", args.flower_number);
            sem_post(semaphore);
            break;
        }
        sem_post(semaphore);
        usleep((rand() % 1000) * 300 + 1000);
    }
    interrupt_handler(-1);
}
