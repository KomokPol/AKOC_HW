#ifndef SHOP_H
#define SHOP_H

// Без этих двух строчек у меня не работало CLOCK_REALTIME
#define _POSIX_C_SOURCE 199309L
#define _XOPEN_SOURCE 600

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <string.h>

#define MAX_CUSTOMERS 100
#define MAX_QUEUE 100
#define SHARED_MEM_NAME "/shop_memory_named"
#define MAX_CUSTOMER_SEMS 100
#define OBSERVER_FIFO "/tmp/shop_observer_fifo"

// Имена именованных семафоров
#define SEM_MUTEX "/shop_mutex"
#define SEM_SELLER1 "/shop_seller1"
#define SEM_SELLER2 "/shop_seller2" 
#define SEM_DAY_OVER "/shop_day_over"
#define SEM_CUSTOMER_BASE "/shop_customer_"

// Структура для разделяемой памяти
typedef struct {
    int queue1[MAX_QUEUE];      // Очередь к первому отделу
    int queue2[MAX_QUEUE];      // Очередь ко второму отделу
    int front1, rear1;          // Указатели для очереди 1
    int front2, rear2;          // Указатели для очереди 2
    int day_over;               // Флаг окончания рабочего дня
    int next_customer_id;       // Счетчик для ID покупателей
    int active_processes;       // Счетчик активных процессов
} shop_t;

// Глобальные переменные для обработки сигналов
extern volatile sig_atomic_t shutdown_flag;
extern shop_t *shop;
extern sem_t *mutex, *seller1_sem, *seller2_sem, *day_over_sem;
extern int observer_fd;

// Прототипы функций
void setup_signal_handlers(void);
void handle_signal(int sig);
void cleanup_resources(void);
int init_shared_resources(void);
sem_t* get_customer_sem(int customer_id, int create_new);
void send_to_observer(const char *message, const char *process_type, int pid);
void init_observer_fifo(void);
void close_observer_fifo(void);

#endif