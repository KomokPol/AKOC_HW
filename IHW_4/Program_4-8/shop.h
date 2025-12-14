#ifndef SHOP_H
#define SHOP_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <semaphore.h>
#include <pthread.h>

#define MAX_CUSTOMERS 100
#define MAX_QUEUE 100

// Структура магазина, которая располагается в процессе и доступна всем потокам
typedef struct {
  int queue1[MAX_QUEUE]; // Очередь к первому отделу (храним id покупателя)
  int queue2[MAX_QUEUE]; // Очередь ко второму отделу
  int front1, rear1; // Указатели для очереди 1
  int front2, rear2; // Указатели для очереди 2
  int day_over; // Флаг окончания рабочего дня (1 это да)
  int shutdown_flag; // Флаг аварийного завершения

  pthread_mutex_t mutex; // Мьютекс для защиты структуры магазина
  sem_t seller1_sem; // Семафор продавца 1
  sem_t seller2_sem; // Семафор продавца 2
  sem_t customer_sems[MAX_CUSTOMERS]; // Семафоры для ожидания конкретного покупателя
} shop_t;

// Глобальный указатель на магазин
extern shop_t *global_shop; // Указатель на структуру магазина
extern FILE *log_fp; // Файловый дескриптор для логов
extern pthread_mutex_t log_mutex; // Мьютекс для потокобезопасного логирования

// Макрос LOG: защищенно печатает в stdout и в файл, если открыт
#define LOG(fmt, ...) do { \
  pthread_mutex_lock(&log_mutex); \
  printf(fmt, ##__VA_ARGS__); \
  if (log_fp) { fprintf(log_fp, fmt, ##__VA_ARGS__); fflush(log_fp); } \
  pthread_mutex_unlock(&log_mutex); \
} while (0)

// Прототипы потоковых функций для pthread_create
void *seller1(void *arg);
void *seller2(void *arg);
void *customer(void *arg);

#endif
