// main.c
#include "shop.h"

shop_t *global_shop = NULL; // Глобальный указатель на разделяемую память для доступа из обработчика сигналов

// Обработчик сигналов
void handle_signal(int sig) {
  printf("\nПринят сигнал %d, отключаемся...\n", sig);
  
  if (global_shop != NULL) {
    global_shop->shutdown_flag = 1; // Устанавливаем флаг в разделяемой памяти
    global_shop->day_over = 1; // Также устанавливаем флаг завершения дня
    
    // Пробуждаем всех продавцов
    sem_post(&global_shop->seller1_sem);
    sem_post(&global_shop->seller2_sem);
    
    // Пробуждаем всех покупателей
    for (int i = 0; i < MAX_CUSTOMERS; i++) {
      sem_post(&global_shop->customer_sems[i]);
    }
  }
}

// Настройка обработчиков сигналов
void setup_signal_handlers(void) {
  signal(SIGINT, handle_signal); // Ctrl+C в термнале
  signal(SIGTERM, handle_signal); // Команда kill в коде
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    printf("Как нужно: %s <число_клиентов>\n", argv[0]);
    return 1;
  }

  int shmid; // Дескриптор разделяемой памяти
  shop_t *shop; // Указатель на магазин в разделяемой памяти
  int num_customers = atoi(argv[1]); // Переводим строку в число
  
  if (num_customers <= 0 || num_customers > MAX_CUSTOMERS) {
    printf("Количество клиентов должно быть от 1 до %d\n", MAX_CUSTOMERS);
    return 1;
  }

  printf("Открытие магазина...\n");
  printf("Число покупателей: %d\n", num_customers);

  // Создаем разделяемую память
  if ( (shmid = shm_open(SHARED_MEM_NAME, O_CREAT | O_RDWR, 0666)) == -1) {
    perror("shm_open");
    return 1;
  }

  // Задаем размер объекта памяти
  if (ftruncate(shmid, sizeof(shop_t)) == -1) {
    perror("ftruncate");
    close(shmid);
    return 1;
  }

  // Отображаем разделяемую память в адресное пространство процесса
  shop = mmap(NULL, sizeof(shop_t), PROT_READ | PROT_WRITE, MAP_SHARED, shmid, 0);
  if (shop == MAP_FAILED) {
    perror("mmap");
    close(shmid);
    return 1;
  }

  // Инициализирем разделяемую память
  shop->front1 = shop->rear1 = 0; // Иниц. указатели очереди 1 отдела
  shop->front2 = shop->rear2 = 0; // Иниц. указатели очереди 2 отдела
  shop->day_over = 0; // Флаг "рабочий день не завершен"
  shop->shutdown_flag = 0; // Флаг экстренного завершения

  global_shop = shop; // Указатель на разделяемую память
  setup_signal_handlers(); // Настройка обработчиков сигналов

  // Инициализируем неименованные семафоры в разделяемой памяти
  if (sem_init(&shop->mutex, 1, 1) == -1) { // Мьютекс доступен (равен 1)
    perror("sem_init mutex");
    return 1;
  }
  if (sem_init(&shop->seller1_sem, 1, 0) == -1) { // Семафор продавца 1 (ждет покупателей)
    perror("sem_init seller1_sem");
    return 1;
  }
  if (sem_init(&shop->seller2_sem, 1, 0) == -1) { // Аналогично для продавца 2
    perror("sem_init seller2_sem");
    return 1;
  }

  // Инициализируем семафоры покупателей
  for (int i = 0; i < MAX_CUSTOMERS; i++) {
    if (sem_init(&shop->customer_sems[i], 1, 0) == -1) {
      perror("sem_init customer_sem");
      return 1;
    }
  }

  // Создаем процессы продавцов
  pid_t seller1_pid = fork();
  if (seller1_pid == -1) {
    perror("fork seller1");
    return 1;
  }
  if (seller1_pid == 0) {
    seller1(shop); // Запускаем процесс продавца 1
    exit(0);
  }

  pid_t seller2_pid = fork();
  if (seller2_pid == -1) {
    perror("fork seller2");
    return 1;
  }
  if (seller2_pid == 0) { // Запускаем процесс продавца 2
    seller2(shop);
    exit(0);
  }

  // Создаем процессы покупателей
  pid_t customer_pids[MAX_CUSTOMERS]; // Массив для хранения pid покупателей
  
  for (int i = 0; i < num_customers; i++) {
    pid_t pid = fork(); // Создаем процесс для каждого покупателя
    if (pid == -1) {
      perror("fork customer");
      return 1;
    }
    if (pid == 0) { // Если процесс создан
      customer(shop, i); // Запускаем процесс покупателя i
      exit(0);
    } else { // Иначе запоминаем pid
      customer_pids[i] = pid;
    }
  }

  // Ожидаем завершение всех покупателей
  for (int i = 0; i < num_customers; i++) {
    int status;
    while (1) {
      if (waitpid(customer_pids[i], &status, 0) == -1) { // Ожидаем завершения процесса
        if (errno == EINTR) continue; // Если процесс был прерван, то повторяем
        perror("waitpid");
        break;
      }
      break;
    }
  }

  printf("-------------------------\n");
  printf("Закрытие магазина...\n");
  
  sem_wait(&shop->mutex); // Захватываем семафор
  shop->day_over = 1; // Устанавливаем флаг завершения рабочего дня
  sem_post(&shop->mutex); // Освобождаем мьютекс

  // Пробуждаем продавцов для завершения работы
  sem_post(&shop->seller1_sem);
  sem_post(&shop->seller2_sem);

  // Ожидаем завершения продавцов
  waitpid(seller1_pid, NULL, 0);
  waitpid(seller2_pid, NULL, 0);

  // Уничтожаем неименованные семафоры
  sem_destroy(&shop->mutex);
  sem_destroy(&shop->seller1_sem);
  sem_destroy(&shop->seller2_sem);
  for (int i = 0; i < MAX_CUSTOMERS; i++) {
    sem_destroy(&shop->customer_sems[i]);
  }

  // Очищаем разделяемую память
  munmap(shop, sizeof(shop_t)); // Отключаем отображение памяти
  shm_unlink(SHARED_MEM_NAME); // Удаляем сегмент разделяемой памяти

  printf("\nМагазин закрыт!\n");
  return 0;
}