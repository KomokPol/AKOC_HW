// Комкова Полина Дмитриевна БПИ244
// Программа: 100 производителей кладут числа 1..100 в общий буфер,
// диспетчер создает сумматоры, суммирующие пары, результат возвращается в буфер,
// завершаем, когда останется один итоговый результат

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

typedef struct Node {
  int value; // Значение в узле
  struct Node *next; // Указатель на следующий узел
} Node;

Node *head = NULL; // Указатель на голову очереди для pop
Node *tail = NULL; // Указатель на хвост очереди для push
int qsize = 0; // Размер очереди

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER; // Мьютекс для доступа к очереди
pthread_cond_t cond = PTHREAD_COND_INITIALIZER; // Условная переменная для диспетчера

// Счетчики для завершения
int producers_left = 100; // Сколько производит. еще не положили значение
int active_summers = 0; // Сколько сумматоров сейчас работают
int final_result = 0; // Результат
int done = 0; // Флаг завершения

void push_back(int v) {
  Node *n = malloc(sizeof(Node)); // Выделяем память под новый узел
  if (!n) { 
    perror("malloc");
    exit(1);
  }

  n->value = v; // Записываем узел
  n->next = NULL; // Указатель на следующий узел
  if (!tail) { // Если очередь пуста
    head = tail = n; 
  } else { // Иначе добавляем в хвост
    tail->next = n;
    tail = n;
  }
  qsize++; // Увеличиваем размер очереди
}
int pop_front() {
  if (!head) {
    fprintf(stderr, "pop from empty\n");
    exit(1);
  }

  Node *n = head; // Сохраняем указатель на старый узел
  int v = n->value; // Запоминаем значение
  head = head->next; // Двигаем голову

  if (!head) {
    tail = NULL;
  }

  free(n); // Освобождаем память
  qsize--; // Уменьшаем размер очереди
  return v; // Возвращаем значение
}

typedef struct {
  int a;
  int b;
  unsigned int seed;
} pair_arg;

void *summer_thread(void *arg) {
  pair_arg *p = (pair_arg*)arg; // Востановливаем структуры
  unsigned int seed = p->seed; // Востановливаем seed
  int a = p->a;
  int b = p->b;
  free(p);

  int delay = (rand_r(&seed) % 4) + 3; // Случайная задержка от 3 до 6
  sleep(delay); // Пауза

  int s = a + b;

  pthread_mutex_lock(&mutex);
  push_back(s); // Кладем в очередь
  active_summers--; // Уменьшаем количество активных сумматоров
  pthread_cond_broadcast(&cond); // Будим диспетчера
  pthread_mutex_unlock(&mutex);

  printf("Сумматор: %d + %d = %d, размер очереди = %d\n", a, b, s, qsize);
  return NULL;
}

void *dispatcher(void *arg) {
  (void)arg;
  while (1) {
    pthread_mutex_lock(&mutex);
    
    // Пока нет двух элементов или не все производители отработали
    while (qsize < 2 && !(producers_left == 0 && active_summers == 0 && qsize == 1)) {
      pthread_cond_wait(&cond, &mutex);
    }

    // Если производители закончили и очередь пуста
    if (producers_left == 0 && active_summers == 0 && qsize == 1) {
      final_result = pop_front(); // Достаем результат
      done = 1;
      pthread_mutex_unlock(&mutex);
      break;
    }

    // Если в очереди хотя бы 2 элемента
    if (qsize >= 2) {
      int x = pop_front();
      int y = pop_front();
      active_summers++; // Увеличиваем количество сумматоров

      pthread_t tid; // Идентификатор потока
      pair_arg *parg = malloc(sizeof(pair_arg)); // Выделяем память
      if (!parg) {
        perror("malloc");
        exit(1);
      }
      
      parg->a = x;
      parg->b = y;
      parg->seed = (unsigned int)time(NULL) ^ (unsigned int)pthread_self();
      
      // Создаем поток-сумматор
      if (pthread_create(&tid, NULL, summer_thread, parg) != 0) { // Если не получилось создать
        perror("pthread_create summer");
        free(parg);
        active_summers--;
      } else { // Успех
        pthread_detach(tid); // Не ждем join
      }
      
      pthread_mutex_unlock(&mutex);
      printf("Диспетчер: запущен сумматор для (%d,%d); Размер очереди = %d; активные сумматоры = %d\n", x, y, qsize, active_summers);
      continue;
    }
    pthread_mutex_unlock(&mutex);
  }
  return NULL;
}

void *producer(void *arg) {
  int id = *((int*)arg); // Получаем id производителя
  unsigned int seed = (unsigned int)time(NULL) ^ (unsigned int)id;

  int delay = (rand_r(&seed) % 7) + 1; // Задержка от 1 до 7
  sleep(delay);

  pthread_mutex_lock(&mutex);
  push_back(id); // Кладем в очередь
  producers_left--; // Этот производитель закончил
  pthread_cond_broadcast(&cond); // Будим диспетчера
  pthread_mutex_unlock(&mutex);

  printf("Производитель %d: положил %d; размер очереди = %d\n", id, id, qsize);
  return NULL;
}

int main(void) {
  srand((unsigned)time(NULL));

  const int N = 100; // Количество производителей
  pthread_t prod[N]; // Массив потоков-производителей
  int ids[N]; // Массив id для передачи в потоки

  for (int i = 0; i < N; ++i) {
    ids[i] = i + 1;
    // Запускаем поток-производителя
    if (pthread_create(&prod[i], NULL, producer, &ids[i]) != 0) {
      perror("pthread_create producer");
      return 1;
    }
  }

  // Запускаем диспетчера
  pthread_t disp;
  if (pthread_create(&disp, NULL, dispatcher, NULL) != 0) {
    perror("pthread_create dispatcher");
    return 1;
  }

  // Ждем завершения всех производителей
  for (int i = 0; i < N; ++i) {
    pthread_join(prod[i], NULL);
  }

  pthread_join(disp, NULL);

  printf("\nРезультат = %d\n", final_result);

  while (head) { // Чистим очередь
    pop_front();
  }

  // Освобождаем ресурсы
  pthread_mutex_destroy(&mutex); 
  pthread_cond_destroy(&cond);
  return 0;
}
