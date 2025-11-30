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

// Именованные семафоры
#define SEM_MUTEX "/shop_mutex"
#define SEM_SELLER1 "/shop_seller1"
#define SEM_SELLER2 "/shop_seller2" 
#define SEM_DAY_OVER "/shop_day_over"
#define SEM_CUSTOMER_BASE "/shop_customer_"

// Структура для разделяемой памяти
typedef struct {
    int queue1[MAX_QUEUE]; // Очередь к первому отделу
    int queue2[MAX_QUEUE]; // Очередь ко второму отделу
    int front1, rear1; // Указатели для очереди 1
    int front2, rear2; // Указатели для очереди 2
    int day_over; // Флаг завершения работы
    int next_customer_id; // Счетчик для ID покупателей
    int active_processes; // Количество активных процессов в системе
} shop_t;

// Глобальные переменные для обработки сигналов
extern volatile sig_atomic_t shutdown_flag;
extern shop_t *shop; // Указатель на разделяемую память

// Именованные семафоры
extern sem_t *mutex; // Мьютекс для доступа к разделяемой памяти
extern sem_t *seller1_sem; // Семафор пробуждения продавца 1
extern sem_t *seller2_sem; // Семафор пробуждения продавца 2
extern sem_t *day_over_sem; // Семафор уведомления о завершении дня

// Прототипы функций
void setup_signal_handlers(void);
void handle_signal(int sig);
void cleanup_resources(void);
int init_shared_resources(void);
sem_t* get_customer_sem(int customer_id, int create_new);

#endif