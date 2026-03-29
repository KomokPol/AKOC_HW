// main.c
#include "shop.h"

shop_t *global_shop = NULL; // Указатель на структуру магазина
FILE *log_fp = NULL; // файловый указатель для логов
pthread_mutex_t log_mutex; // Мьютекс для синхронизации вывода логов в разные потоки

// Обработчик сигналов аварийного завершения
void handle_signal(int sig) {
  const char msg[] = "\nПринят сигнал, выполняю аварийное завершение...\n";
  write(STDOUT_FILENO, msg, sizeof(msg) - 1); // Печатаем в консоль
  if (global_shop != NULL) {
    // Устанавливаем флаг в разделяемой памяти
    global_shop->shutdown_flag = 1;
    global_shop->day_over = 1;

    // Пробуждаем продавцов
    sem_post(&global_shop->seller1_sem);
    sem_post(&global_shop->seller2_sem);

    // Пробуждаем всех покупателей
    for (int i = 0; i < MAX_CUSTOMERS; i++) {
      sem_post(&global_shop->customer_sems[i]);
    }
  }
}

// Настройка обработчика сигналов
void setup_signal_handlers(void) {
  signal(SIGINT, handle_signal);
  signal(SIGTERM, handle_signal);
}

// Чтение конфигурационного файла простого формата: ключ=значение
int read_config_file(const char *fname, int *num_customers_out, char **output_out) {
  FILE *f = fopen(fname, "r"); // Открываем файл
  if (!f) {
    return -1;
  }

  char line[256]; // Буфер для строки
  while (fgets(line, sizeof(line), f)) { // Читаем построчно
    char *p = strchr(line, '#'); // Убираем комментарии
    if (p) { // Если комментарии есть
      *p = '\0';
    }

    char *eq = strchr(line, '='); // Ищем равенство
    if (!eq) { // Если нет =, пропускаем строку
      continue;
    }
    *eq = '\0'; // Разделяем строку на ключ и значение
    char *key = line; // Ключ
    char *val = eq + 1; // Значение

    // Обрезаем пробелы и переводы строк
    char *end;

    // Работаем с ключом
    while (*key == ' ' || *key == '\t') {
      key++;
    }
    end = key + strlen(key) - 1; // Указатель на конец ключа
    while (end > key && (*end == ' ' || *end == '\t')) {
      *end = '\0';
      end--;
    }
    // Работае со значением
    while (*val == ' ' || *val == '\t') {
      val++;
    }
    end = val + strlen(val) - 1; // Указатель на конец значения
    while (end > val && (*end == '\n' || *end == '\r' || *end == ' ' || *end == '\t')) { 
      *end = '\0';
      end--;
    }

    if (strcmp(key, "num_customers") == 0) { // Если ключ num_customers
      *num_customers_out = atoi(val); // Переводим строку в число
    } else if (strcmp(key, "output") == 0) { // Если ключ output
      *output_out = strdup(val); // Создаем копию строки
    }
  }
  fclose(f); // Закрываем файл
  return 0;
}

// Метод с подсказками при неверном вводе
void print_usage(const char *prog) {
  printf("Использование:\n");
  printf("ИЛИ %s -n N -o outfile, где N = число клиентов\n", prog);
  printf("ИЛИ %s -c configfile -o outfile, где configfile содержит num_customers=NN и опционально output=...\n", prog);
  printf("ИЛИ вариант без вывода в файл: %s N, где N = число клиентов\n", prog);
}


int main(int argc, char *argv[]) {
  int num_customers = -1; // Число покупателей
  char *cfg_file = NULL; // Имя конфигурационного файла
  char *out_name = NULL; // Имя файла для логов

  // Парсим аргументы: поддерживаю -n, -c, -o, а также просто число
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) { // Если параметр -n и следующий аргумент есть
      num_customers = atoi(argv[++i]); // Переводим строку в число
    } else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
      cfg_file = argv[++i];
    } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
      out_name = argv[++i];
    } else if (argv[i][0] == '-') {
      print_usage(argv[0]);
      return 1;
    } else { 
      if (num_customers == -1) {
        num_customers = atoi(argv[i]);
      } else {
        print_usage(argv[0]);
        return 1; 
      }
    }
  }

  if (cfg_file) { // Если передан -c configfile
    if (read_config_file(cfg_file, &num_customers, &out_name) != 0) { // Если он не читается
      fprintf(stderr, "Не удалось прочитать конфигурационный файл %s\n", cfg_file);
      return 1;
    }
  }

  if (num_customers <= 0 || num_customers > MAX_CUSTOMERS) {
    print_usage(argv[0]); // Выводим подсказку
    fprintf(stderr, "Число клиентов должно быть в диапазоне 1..%d\n", MAX_CUSTOMERS);
    return 1;
  }

  if (out_name) { // Если задано имя файла вывода
    log_fp = fopen(out_name, "w"); // Открываем файл на запись или перезапись
    if (!log_fp) { // Если ошибка
      perror("fopen output file");
      return 1;
    }
  } else {
    log_fp = NULL;  // Иначе лог-файл не используется
  }

  if (pthread_mutex_init(&log_mutex, NULL) != 0) { // Создаем мьютекс для защиты логов
    perror("pthread_mutex_init log_mutex");
    return 1;
  }

  LOG("Открытие магазина...\n");
  LOG("Число покупателей: %d\n", num_customers);
  if (out_name) {
    LOG("Логи будут записываться в файл: %s\n", out_name); // Сообщаем имя файла, если есть
  }

  // Выделяем память под структуру магазина в общем адресном пространстве процесса
  shop_t *shop = malloc(sizeof(shop_t));
  if (!shop) { // Если не удалось выделить память
    perror("malloc shop");
    return 1; 
  }

  // Инициализируем структуру магазина
  shop->front1 = shop->rear1 = 0;
  shop->front2 = shop->rear2 = 0;
  shop->day_over = 0;
  shop->shutdown_flag = 0;

  global_shop = shop; // Созраняем глобально для handler и потоков
  setup_signal_handlers(); // Регистрируем обработчики сигналов

  // Инициализируем семафоры
  if (pthread_mutex_init(&shop->mutex, NULL) != 0) { perror("sem_init mutex"); return 1; }
  if (sem_init(&shop->seller1_sem, 0, 0) == -1) { perror("sem_init seller1"); return 1; }
  if (sem_init(&shop->seller2_sem, 0, 0) == -1) { perror("sem_init seller2"); return 1; }

  for (int i = 0; i < MAX_CUSTOMERS; i++) {
    if (sem_init(&shop->customer_sems[i], 0, 0) == -1) {
      perror("sem_init customer");
      return 1;
    }
  }

  // Создаем потоки-продавцы
  pthread_t seller1_tid;
  pthread_t seller2_tid;
  if (pthread_create(&seller1_tid, NULL, seller1, shop) != 0) { perror("pthread_create seller1"); return 1; }
  if (pthread_create(&seller2_tid, NULL, seller2, shop) != 0) { perror("pthread_create seller2"); return 1; }

  // Создаем потоки-покупатели
  pthread_t customer_tids[MAX_CUSTOMERS];
  int ids[MAX_CUSTOMERS];
  for (int i = 0; i < num_customers; i++) {
    ids[i] = i + 1; // ID покупателя + 1 для нумерации
    if (pthread_create(&customer_tids[i], NULL, customer, &ids[i]) != 0) {
      perror("pthread_create customer");
      return 1;
    }
    // Небольшаяпауза между созданинями потоков для вывода в консоль
    usleep(1000);
  }

  // Ждем завершения всех покупателей через pthread_join
  for (int i = 0; i < num_customers; i++) {
    pthread_join(customer_tids[i], NULL);
  }

  LOG("-------------------------\n");
  LOG("Закрытие магазина...\n");

  // Сообщаем продавцам о завершении дня
  pthread_mutex_lock(&shop->mutex);
  shop->day_over = 1;
  shop->front1 = shop->rear1 = 0; // Очищаем очереди
  shop->front2 = shop->rear2 = 0;
  pthread_mutex_unlock(&shop->mutex);

  // Пробуждаем продавцов, чтобы они вышли из ожидания и завершились
  sem_post(&shop->seller1_sem);
  sem_post(&shop->seller2_sem);

  // Ждем завершения потоков-продавцов
  pthread_join(seller1_tid, NULL);
  pthread_join(seller2_tid, NULL);

  // Уничтожаем семафоры
  pthread_mutex_destroy(&shop->mutex);
  for (int i = 0; i < MAX_CUSTOMERS; i++) {
    sem_destroy(&shop->customer_sems[i]);
  }
  sem_destroy(&shop->seller1_sem);
  sem_destroy(&shop->seller2_sem);

  if (log_fp) { // Если лог-файл был открыт
    fclose(log_fp); // Закрываем
    log_fp = NULL; // Обнуляем
  }
  pthread_mutex_destroy(&log_mutex); // Разрушаем мьютекс логирования

  // Освобождаем память
  free(shop);
  global_shop = NULL;

  LOG("\nМагазин закрыт!\n");
  return 0;
}
