// shop.c
#include "shop.h"

// Глобальные переменные
volatile sig_atomic_t shutdown_flag = 0;
shop_t *shop = NULL;
sem_t *mutex = NULL, *seller1_sem = NULL, *seller2_sem = NULL, *day_over_sem = NULL;
int shm_fd = -1; // Дескриптор разделяемой памяти

// Обработчик сигналов
void handle_signal(int sig) {
    printf("\nПроцесс %d: Получен сигнал %d, завершаем работу...\n", getpid(), sig);
    shutdown_flag = 1; // Флаг завершения
}

// Настройка обработчиков сигналов
void setup_signal_handlers(void) {
    signal(SIGINT, handle_signal); // Ctrl+C в термнале
    signal(SIGTERM, handle_signal); // Команда kill в коде
}

// Получение семафора покупателя
sem_t* get_customer_sem(int customer_id, int create_new) {
    char sem_name[50];
    snprintf(sem_name, sizeof(sem_name), "%s%d", SEM_CUSTOMER_BASE, customer_id); // Формируем уникальное имя семафора для покупателя
    
    sem_t *customer_sem;
    if (create_new) { // Если покупатель создает свой семафор
        customer_sem = sem_open(sem_name, O_CREAT | O_EXCL, 0666, 0); // Создаем
        if (customer_sem == SEM_FAILED && errno == EEXIST) {
            customer_sem = sem_open(sem_name, 0); // Если вдруг два процесса захотели создать один и тот же семафор
        }
    } else { // Если продавец ищет семафор покупателя для уведомления
        customer_sem = sem_open(sem_name, 0); // Просто открываем существующий
    }
    
    if (customer_sem == SEM_FAILED) { // Если не получилось созать/открыть семафор
        perror("sem_open customer");
        return NULL;
    }
    return customer_sem;
}

// Инициализация разделяемых ресурсов
int init_shared_resources(void) {
    int created = 0;

    shm_fd = shm_open(SHARED_MEM_NAME, O_CREAT | O_EXCL | O_RDWR, 0666);
    if (shm_fd >= 0) { // Если смогли создать память
        created = 1;
    } else if (errno == EEXIST) { // Если память уже существует
        shm_fd = shm_open(SHARED_MEM_NAME, O_RDWR, 0666);
    } 

    if (shm_fd == -1) {
        perror("shm_open");
        return -1;
    }

    if (created) { // Если память только что создана
        // Задаем размер объекта памяти
        if (ftruncate(shm_fd, sizeof(shop_t)) == -1) {
            perror("ftruncate");
            close(shm_fd);
            return -1;
        }
    }

    // Отображаем разделяемую память в адресное пространство процесса
    shop = mmap(NULL, sizeof(shop_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shop == MAP_FAILED) {
        perror("mmap");
        close(shm_fd);
        return -1;
    }

    // Создаем/открываем семафоры
    mutex = sem_open(SEM_MUTEX, O_CREAT, 0666, 1);
    seller1_sem = sem_open(SEM_SELLER1, O_CREAT, 0666, 0);
    seller2_sem = sem_open(SEM_SELLER2, O_CREAT, 0666, 0);
    day_over_sem = sem_open(SEM_DAY_OVER, O_CREAT, 0666, 0);

    // Если что-то пошло не так
    if (mutex == SEM_FAILED || seller1_sem == SEM_FAILED || seller2_sem == SEM_FAILED || day_over_sem == SEM_FAILED) {
        perror("sem_open");
        if (shop) { // Если разделяемая память была создана
            munmap(shop, sizeof(shop_t)); // Освобождаем
            shop = NULL;
        }
        close(shm_fd);
        return -1;
    }

    if (created) { // Если паямть только была создана
        sem_wait(mutex);
        memset(shop, 0, sizeof(shop_t)); // Обнуляем структуру
        shop->active_processes = 0; // Инициализируем счетчик процессов
        shop->next_customer_id = 0; // Начинаем нумерацию покупателей с 0
        sem_post(mutex);
    }
    return 0;
}

// Очистка ресурсов
void cleanup_resources(void) {
    int last = 0; // Последний ли это процесс
    if (!shop) return; // Если разделяемая память не инициализирована, скип

    if (mutex != NULL && mutex != SEM_FAILED) { // Если семафоры инициализированы
        if (sem_wait(mutex) == 0) {
            if (shop) {
                shop->active_processes--;
                if (shop->active_processes <= 0) { // Если не осталось процессов
                    shop->active_processes = 0; // Обнуляем счетчик
                    last = 1; // Последний процесс
                }
            }
            sem_post(mutex);
        } else {
            perror("sem_wait in cleanup");
        }
    }

    // Закрываем дескрипторы семафоров
    if (mutex) { sem_close(mutex); mutex = NULL; }
    if (seller1_sem) { sem_close(seller1_sem); seller1_sem = NULL; }
    if (seller2_sem) { sem_close(seller2_sem); seller2_sem = NULL; }
    if (day_over_sem) { sem_close(day_over_sem); day_over_sem = NULL; }

    // Освобождаем разделяемую память
    if (shop) {
        munmap(shop, sizeof(shop_t));
        shop = NULL;
    }
    // Закрываем дескриптор
    if (shm_fd != -1) {
        close(shm_fd);
        shm_fd = -1;
    }

    if (last) { // Если это последний процесс
        // Удаляем объекты из системы
        shm_unlink(SHARED_MEM_NAME);
        sem_unlink(SEM_MUTEX);
        sem_unlink(SEM_SELLER1);
        sem_unlink(SEM_SELLER2);
        sem_unlink(SEM_DAY_OVER);

        // Удаляем семафоры покупателей
        for (int i = 0; i < MAX_CUSTOMER_SEMS; ++i) {
            char sem_name[64];
            snprintf(sem_name, sizeof(sem_name), "%s%d", SEM_CUSTOMER_BASE, i);
            sem_unlink(sem_name);
        }
    }
    printf("Процесс %d: Очистка завершена\n", getpid());
}
