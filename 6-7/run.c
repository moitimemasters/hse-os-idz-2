#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <wait.h>

#define NUMBER_OF_FLOWERS 10
#define NUMBER_OF_GARDENERS 2

typedef struct {
    sem_t init_semaphore;
    sem_t semaphore;
    int flowers[NUMBER_OF_FLOWERS];
} shared_data_t;

shared_data_t *shared_data;

void flower(int flower_number) {
    srand(time(NULL));

    sem_wait(&shared_data->init_semaphore);
    sem_post(&shared_data->init_semaphore);

    int *flower_state = &shared_data->flowers[flower_number];

    while (true) {
        sem_wait(&shared_data->semaphore);
        if (*flower_state == 0) {
            if (rand() % 5 == 0) {
                *flower_state = -1;
                printf("Цветок #%d начинает вять.\n", flower_number);
            }
        } else if (*flower_state == 1) {
            *flower_state = 0;
            printf("Цветок #%d полит.\n", flower_number);
        } else if (*flower_state == -1) {
            (*flower_state)--;
        } else if (*flower_state < -1) {
            printf("Цветок #%d увял.\n", flower_number);
            sem_post(&shared_data->semaphore);
            break;
        } else if (*flower_state > 1) {
            printf("Цветок #%d перелит.\n", flower_number);
            sem_post(&shared_data->semaphore);
            break;
        }
        sem_post(&shared_data->semaphore);
        usleep((rand() % 1000) * 300 + 1000);
    }
}

void gardener(int gardener_number) {
    srand(time(NULL));

    sem_wait(&shared_data->init_semaphore);
    sem_post(&shared_data->init_semaphore);

    while (true) {
        int count_dead = 0;
        for (int i = 0; i < NUMBER_OF_FLOWERS; ++i) {
            sem_wait(&shared_data->semaphore);
            if (shared_data->flowers[i] == -1) {
                shared_data->flowers[i] += 2;
                printf("Садовник #%d поливает цветок #%d\n", gardener_number,
                       i);
            }
            if (shared_data->flowers[i] < -1 || shared_data->flowers[i] > 1) {
                count_dead++;
            }
            sem_post(&shared_data->semaphore);
            usleep((rand() % 500) * 100 + 1000);
        }
        if (count_dead == NUMBER_OF_FLOWERS) {
            printf("Садовник #%d уходит домой.\n", gardener_number);
            break;
        }
    }
}

void signal_handler(int signum) {
    if (signum == SIGINT || signum == SIGTSTP) {
        sem_destroy(&shared_data->semaphore);
        sem_destroy(&shared_data->init_semaphore);
        munmap(shared_data, sizeof(shared_data_t));
        exit(EXIT_SUCCESS);
    }
}

int main() {
    shared_data = (shared_data_t *)mmap(NULL, sizeof(shared_data_t),
                                        PROT_READ | PROT_WRITE,
                                        MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    sem_init(&shared_data->init_semaphore, 1, 0);
    sem_init(&shared_data->semaphore, 1, 1);

    signal(SIGINT, signal_handler);
    signal(SIGTSTP, signal_handler);

    for (int i = 0; i < NUMBER_OF_FLOWERS; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        }
        if (pid == 0) {
            flower(i);
            exit(EXIT_SUCCESS);
        }
    }

    for (int i = 0; i < NUMBER_OF_GARDENERS; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            gardener(i);
            exit(EXIT_SUCCESS);
        }
    }

    sem_post(&shared_data->init_semaphore);

    while (wait(NULL) > 0)
        ;

    signal_handler(SIGINT);
}
