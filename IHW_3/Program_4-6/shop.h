#ifndef SHOP_H
#define SHOP_H

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
#include <fcntl.h>

#define MAX_CUSTOMERS 100
#define MAX_QUEUE 100
#define SHARED_MEM_NAME "/shop_memory"

// Структура для разделяемой памяти
typedef struct {
  int queue1[MAX_QUEUE]; // Очередь к первому отделу
  int queue2[MAX_QUEUE]; // Очередь ко второму отделу
  int front1, rear1; // Указатели для очереди 1
  int front2, rear2; // Указатели для очереди 2
  int day_over; // Флаг окончания рабочего дня
  int shutdown_flag; // Флаг экстренного завершения
  
  sem_t mutex; // Мьютекс для защиты разделяемой памяти
  sem_t seller1_sem; // Семафор продавца 1
  sem_t seller2_sem; // Семафор продавца 2
  sem_t customer_sems[MAX_CUSTOMERS]; // Семафоры покупателей
} shop_t;

// Прототипы функций
void seller1(shop_t *shop);
void seller2(shop_t *shop);
void customer(shop_t *shop, int index);

#endif