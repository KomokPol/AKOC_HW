#ifndef SHOP_ALT_H
#define SHOP_ALT_H

// Без этих двух строчек у меня не работало CLOCK_REALTIME
#define _POSIX_C_SOURCE 199309L
#define _XOPEN_SOURCE 600

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdatomic.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <semaphore.h>
#include <stdint.h>

#define MAX_CUSTOMERS 100
#define MAX_QUEUE 100

// Структура магазина аналогична shop_t, но с условными переменными
typedef struct {
  int queue1[MAX_QUEUE]; // Очередь к первому отделу (храним id покупателя)
  int queue2[MAX_QUEUE]; // Очередь ко второму отделу
  int front1, rear1; // Указатели для очереди 1
  int front2, rear2; // Указатели для очереди 2

  pthread_mutex_t mutex; // Мьютекс для защиты общих данных
  pthread_cond_t seller1_cond; // Условная переменная для продавца 1 
  pthread_cond_t seller2_cond; // Условная переменная для продавца 2 

  pthread_cond_t customer_conds[MAX_CUSTOMERS]; // Условные перем. для покупателей
  pthread_mutex_t customer_mutexes[MAX_CUSTOMERS]; // Мьютексы для customer_conds
  int customer_flags[MAX_CUSTOMERS]; // Флаги пробуждения покупателей 

  atomic_int shutdown_flag; // Флаг аварийного завершения
  int day_over; // Флаг окончания рабочего дня (1 это да)
} shop_alt_t;

extern shop_alt_t *global_shop_alt; // Указатель на структуру магазина
extern FILE *log_fp_alt; // Файловый дескриптор для логов
extern pthread_mutex_t log_mutex_alt; // Мьютекс для потокобезопасного логирования

// Макрос LOGA: защищенно печатает в stdout и в файл, если открыт
#define LOGA(fmt, ...) do { \
  pthread_mutex_lock(&log_mutex_alt); \
  printf(fmt, ##__VA_ARGS__); \
  if (log_fp_alt) { fprintf(log_fp_alt, fmt, ##__VA_ARGS__); fflush(log_fp_alt); } \
  pthread_mutex_unlock(&log_mutex_alt); \
} while (0)

// Это LCG: возвращает 31-битное значение
static unsigned int thread_rand(unsigned int *seed) {
  *seed = (1103515245u * (*seed) + 12345u); // Это POSIX lrand48-like
  return (*seed >> 1); // Обрезаем младший бит, чтобы получить положительное число
}

// Прототипы потоковых функций
void *seller1_alt(void *arg);
void *seller2_alt(void *arg);
void *customer_alt(void *arg);

#endif
