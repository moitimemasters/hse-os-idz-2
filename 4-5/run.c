#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "common.h"

const int NUMBER_OF_FLOWERS = 10;
const int NUMBER_OF_GARDENERS = 2;

sem_t *initialization_sem, *semaphore;
int *shared_mem;
int shared_mem_fd;

void handle_signal(int sig) {
    if (sig == SIGINT || sig == SIGTSTP) {
        sem_unlink("initialization_sem");
        sem_close(initialization_sem);
        sem_unlink(SEMAPHORE_NAME);
        sem_close(semaphore);
        munmap(shared_mem, sizeof(int) * NUMBER_OF_FLOWERS);
        close(shared_mem_fd);
        shm_unlink(SHARED_MEM_NAME);
        exit(EXIT_SUCCESS);
    }
}

int main() {
    // Устанавливаем обработчики сигналов
    signal(SIGINT, handle_signal);
    signal(SIGTSTP, handle_signal);

    // Создаем семафор инициализации и блокируем его
    initialization_sem = sem_open("initialization_sem", O_CREAT, S_IRWXU, 0);
    if (initialization_sem == SEM_FAILED) {
        perror("sem_open/initialization_sem");
        exit(EXIT_FAILURE);
    }
    // Созжаем общий семафор и НЕ блокируем его
    semaphore = sem_open(SEMAPHORE_NAME, O_CREAT, S_IRWXU, 1);
    if (semaphore == SEM_FAILED) {
        perror("sem_open/SEMAPHORE_NAME");
        exit(EXIT_FAILURE);
    }

    // Создаем и инициализируем общую память
    shared_mem_fd = shm_open(SHARED_MEM_NAME, O_CREAT | O_RDWR, S_IRWXU);
    if (shared_mem_fd == -1) {
        perror("shm_open/shared_mem_fd");
        exit(EXIT_FAILURE);
    }

    if (ftruncate(shared_mem_fd, sizeof(int) * NUMBER_OF_FLOWERS) == -1) {
        perror("ftruncate");
        close(shared_mem_fd);
        exit(EXIT_FAILURE);
    }

    shared_mem = mmap(NULL, sizeof(int) * NUMBER_OF_FLOWERS,
                      PROT_READ | PROT_WRITE, MAP_SHARED, shared_mem_fd, 0);
    if (shared_mem == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    // Инициализируем общую память,
    for (int i = 0; i < NUMBER_OF_FLOWERS; i++) {
        shared_mem[i] = 0;
    }

    // Запускаем процессы-цветы и процессы-садовники
    for (int i = 0; i < NUMBER_OF_FLOWERS; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            char index[10];
            sprintf(index, "%d", i);
            char total[10];
            sprintf(total, "%d", NUMBER_OF_FLOWERS);
            char *args[] = {"./flower", "-n", index, "-t", total, NULL};
            if (execv(args[0], args) < 0) {
                perror("execv");
                exit(EXIT_FAILURE);
            }
            exit(EXIT_SUCCESS);
        }
    }

    for (int i = 0; i < NUMBER_OF_GARDENERS; i++) {
        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            char index[10];
            sprintf(index, "%d", i);
            char total[10];
            sprintf(total, "%d", NUMBER_OF_FLOWERS);
            char *args[] = {"./gardener", "-n", index, "-t", total, NULL};
            if (execv(args[0], args) < 0) {
                perror("execv");
                exit(EXIT_FAILURE);
            }
            exit(EXIT_SUCCESS);
        }
    }

    // Все процессы запущены, теперь можно разблокировать семафор инициализации
    sem_post(initialization_sem);

    // Ждем завершения всех дочерних процессов
    while (wait(NULL) > 0)
        ;

    // Удаление ресурсов
    sem_close(initialization_sem);
    sem_unlink("initialization_sem");
    sem_close(semaphore);
    sem_unlink(SEMAPHORE_NAME);
    munmap(shared_mem, sizeof(int) * NUMBER_OF_FLOWERS);
    close(shared_mem_fd);
    shm_unlink(SHARED_MEM_NAME);

    return EXIT_SUCCESS;
}
