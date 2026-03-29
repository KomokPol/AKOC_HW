#include "shop.h"

shop_alt_t *global_shop_alt = NULL; // Указатель на структуру магазина
FILE *log_fp_alt = NULL; // файловый указатель для логов
pthread_mutex_t log_mutex_alt = PTHREAD_MUTEX_INITIALIZER; // Мьютекс для синхронизации вывода логов в разные потоки

// Обработчик сигналов, где безопасно устанавливаем флаг shutdown
void handle_signal_alt(int sig) {
    const char msg[] = "\nПринят сигнал, выполняю аварийное завершение...\n";
    write(STDOUT_FILENO, msg, sizeof(msg) - 1); // Печатаем в консоль
    if (global_shop_alt) {
        atomic_store(&global_shop_alt->shutdown_flag, 1); // Устанавливаем флаг в разделяемой памяти
    }
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

// Инициализация customer cond и mutex
static void init_customer(shop_alt_t *shop) {
    for (int i = 0; i < MAX_CUSTOMERS; ++i) {
        pthread_mutex_init(&shop->customer_mutexes[i], NULL);
        pthread_cond_init(&shop->customer_conds[i], NULL);
        shop->customer_flags[i] = 0;
    }
}

// Уничтожение customer cond и mutex
static void destroy_customer(shop_alt_t *shop) {
    for (int i = 0; i < MAX_CUSTOMERS; ++i) {
        pthread_mutex_destroy(&shop->customer_mutexes[i]);
        pthread_cond_destroy(&shop->customer_conds[i]);
    }
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

    if (cfg_file) {
        if (read_config_file(cfg_file, &num_customers, &out_name) != 0) {
            fprintf(stderr, "Не удалось прочитать конфигурационный файл %s\n", cfg_file);
            return 1;
        }
    }

    if (num_customers <= 0 || num_customers > MAX_CUSTOMERS) {
        fprintf(stderr, "Число клиентов должно быть 1..%d\n", MAX_CUSTOMERS);
        return 1;
    }

    if (out_name) {
        log_fp_alt = fopen(out_name, "w");
        if (!log_fp_alt) { 
            perror("fopen");
            return 1;
        }
    } else {
        log_fp_alt = NULL;
    }

    if (pthread_mutex_init(&log_mutex_alt, NULL) != 0) { // Создаем мьютекс для защиты логов
        perror("pthread_mutex_init log_mutex");
        return 1;
    }

    LOGA("Открытие магазина...\n");
    LOGA("Число покупателей: %d\n", num_customers);
    if (out_name) LOGA("Логи будут записываться в файл: %s\n", out_name);

    // Инициализация магазина
    shop_alt_t *shop = malloc(sizeof(shop_alt_t)); // Выделяем память под структуру
    if (!shop) { 
        perror("malloc");
        return 1;
    }

    memset(shop, 0, sizeof(*shop)); // Обнуляем структуру
    pthread_mutex_init(&shop->mutex, NULL); // Создаем мьютекс
    pthread_cond_init(&shop->seller1_cond, NULL); // Создаем условную переменную для продавца 1
    pthread_cond_init(&shop->seller2_cond, NULL); // Создаем условную переменную для продавца 2
    shop->front1 = shop->rear1 = 0;
    shop->front2 = shop->rear2 = 0;
    atomic_init(&shop->shutdown_flag, 0);
    shop->day_over = 0;

    init_customer(shop); // Инициализация customer cond и mutex
    global_shop_alt = shop; // Указатель на структуру магазина

    signal(SIGINT, handle_signal_alt);
    signal(SIGTERM, handle_signal_alt);

    // Создаем продавцов
    pthread_t s1;
    pthread_t s2;
    if (pthread_create(&s1, NULL, seller1_alt, shop) != 0) { perror("pthread_create s1"); return 1; }
    if (pthread_create(&s2, NULL, seller2_alt, shop) != 0) { perror("pthread_create s2"); return 1; }

    // Создаем покупателей
    pthread_t customers[MAX_CUSTOMERS];
    int ids[MAX_CUSTOMERS];
    for (int i = 0; i < num_customers; ++i) {
        ids[i] = i + 1;
        if (pthread_create(&customers[i], NULL, customer_alt, &ids[i]) != 0) {
            perror("pthread_create cust");
            return 1;
        }
        // Небольшаяпауза между созданинями потоков для вывода в консоль
        struct timespec ts = {0, 1000000};
        nanosleep(&ts, NULL);
    }

    // Ждем завершения покупателей
    for (int i = 0; i < num_customers; ++i) {
        pthread_join(customers[i], NULL);
    }

    LOGA("-------------------------\n");
    LOGA("Закрытие магазина...\n");

    // Устанавливаем day_over и пробуждаем продавцов, чтобы они могли завершиться 
    pthread_mutex_lock(&shop->mutex);
    shop->day_over = 1;
    pthread_mutex_unlock(&shop->mutex);

    pthread_cond_signal(&shop->seller1_cond);
    pthread_cond_signal(&shop->seller2_cond);

    // Ждем завершения продавцов
    pthread_join(s1, NULL);
    pthread_join(s2, NULL);

    // Очищаем память
    destroy_customer(shop);
    pthread_mutex_destroy(&shop->mutex);
    pthread_cond_destroy(&shop->seller1_cond);
    pthread_cond_destroy(&shop->seller2_cond);

    if (log_fp_alt) {
        fclose(log_fp_alt);
        log_fp_alt = NULL;
    }
    pthread_mutex_destroy(&log_mutex_alt);

    free(shop);
    global_shop_alt = NULL;

    LOGA("\nМагазин закрыт!\n");
    return 0;
}
