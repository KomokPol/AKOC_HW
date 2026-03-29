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
    shutdown_flag = 1;
}

// Настройка обработчиков сигналов
void setup_signal_handlers(void) {
    signal(SIGINT, handle_signal); // Ctrl+C в термнале
    signal(SIGTERM, handle_signal); // Команда kill в коде
}

// Получение семафора покупателя
sem_t* get_customer_sem(int customer_id, int create_new) {
    char sem_name[50];
    snprintf(sem_name, sizeof(sem_name), "%s%d", SEM_CUSTOMER_BASE, customer_id);
    
    sem_t *customer_sem;
    if (create_new) {
        customer_sem = sem_open(sem_name, O_CREAT | O_EXCL, 0666, 0);
        if (customer_sem == SEM_FAILED && errno == EEXIST) {
            customer_sem = sem_open(sem_name, 0);
        }
    } else {
        customer_sem = sem_open(sem_name, 0);
    }
    
    if (customer_sem == SEM_FAILED) {
        perror("sem_open customer");
        return NULL;
    }
    return customer_sem;
}

// Регистрация нового наблюдателя
void register_observer(int pid) {
    if (!shop) return;
    
    sem_wait(mutex);
    if (shop->observer_count < MAX_OBSERVERS) {
        shop->observer_pids[shop->observer_count] = pid;
        shop->observer_count++;
        printf("Зарегистрирован наблюдатель PID: %d\n", pid);
    }
    sem_post(mutex);
}

// Удаление наблюдателя
void unregister_observer(int pid) {
    if (!shop) return;
    
    sem_wait(mutex);
    for (int i = 0; i < shop->observer_count; i++) {
        if (shop->observer_pids[i] == pid) {
            // Сдвигаем массив
            for (int j = i; j < shop->observer_count - 1; j++) {
                shop->observer_pids[j] = shop->observer_pids[j + 1];
            }
            shop->observer_count--;
            printf("Удален наблюдатель PID: %d\n", pid);
            break;
        }
    }
    sem_post(mutex);
}

// Отправка сообщения всем наблюдателям
void send_to_observers(const char *message, const char *process_type, int pid) {
    if (!shop || shop->observer_count == 0) return;
    
    char buffer[512];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%H:%M:%S", tm_info);
    
    snprintf(buffer, sizeof(buffer), "[%s] %s (PID: %d): %s\n", timestamp, process_type, pid, message);
    
    // Отправляем сообщение всем зарегистрированным наблюдателям
    sem_wait(mutex);
    for (int i = 0; i < shop->observer_count; i++) {
        char fifo_name[64];
        snprintf(fifo_name, sizeof(fifo_name), "%s%d", OBSERVER_FIFO_BASE, shop->observer_pids[i]);
        
        int fd = open(fifo_name, O_WRONLY | O_NONBLOCK);
        if (fd != -1) {
            write(fd, buffer, strlen(buffer));
            close(fd);
        }
    }
    sem_post(mutex);
}

// Инициализация разделяемых ресурсов
int init_shared_resources(void) {
    int created = 0;
    shm_fd = shm_open(SHARED_MEM_NAME, O_CREAT | O_EXCL | O_RDWR, 0666);
    if (shm_fd >= 0) {
        created = 1;
    } else if (errno == EEXIST) {
        shm_fd = shm_open(SHARED_MEM_NAME, O_RDWR, 0666);
    } 

    if (shm_fd == -1) {
        perror("shm_open");
        return -1;
    }

    if (created) {
        if (ftruncate(shm_fd, sizeof(shop_t)) == -1) {
            perror("ftruncate");
            close(shm_fd);
            return -1;
        }
    }

    shop = mmap(NULL, sizeof(shop_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shop == MAP_FAILED) {
        perror("mmap");
        close(shm_fd);
        return -1;
    }

    mutex = sem_open(SEM_MUTEX, O_CREAT, 0666, 1);
    seller1_sem = sem_open(SEM_SELLER1, O_CREAT, 0666, 0);
    seller2_sem = sem_open(SEM_SELLER2, O_CREAT, 0666, 0);
    day_over_sem = sem_open(SEM_DAY_OVER, O_CREAT, 0666, 0);

    if (mutex == SEM_FAILED || seller1_sem == SEM_FAILED ||
        seller2_sem == SEM_FAILED || day_over_sem == SEM_FAILED) {
        perror("sem_open");
        if (shop) { munmap(shop, sizeof(shop_t)); shop = NULL; }
        close(shm_fd);
        return -1;
    }

    if (created) {
        sem_wait(mutex);
        memset(shop, 0, sizeof(shop_t));
        shop->initialized = 1;
        shop->active_processes = 0;
        shop->next_customer_id = 0;
        shop->observer_count = 0;
        sem_post(mutex);
    }

    return 0;
}

// Очистка ресурсов
void cleanup_resources(void) {
    int last = 0;
    if (!shop) return;

    if (mutex != NULL && mutex != SEM_FAILED) {
        if (sem_wait(mutex) == 0) {
            if (shop) {
                shop->active_processes--;
                if (shop->active_processes <= 0) {
                    shop->active_processes = 0;
                    last = 1;
                    shop->initialized = 0;
                }
            }
            sem_post(mutex);
        } else {
            perror("sem_wait in cleanup");
        }
    }

    if (mutex) { sem_close(mutex); mutex = NULL; }
    if (seller1_sem) { sem_close(seller1_sem); seller1_sem = NULL; }
    if (seller2_sem) { sem_close(seller2_sem); seller2_sem = NULL; }
    if (day_over_sem) { sem_close(day_over_sem); day_over_sem = NULL; }

    if (shop) {
        munmap(shop, sizeof(shop_t));
        shop = NULL;
    }
    if (shm_fd != -1) {
        close(shm_fd);
        shm_fd = -1;
    }

    if (last) {
        shm_unlink(SHARED_MEM_NAME);
        sem_unlink(SEM_MUTEX);
        sem_unlink(SEM_SELLER1);
        sem_unlink(SEM_SELLER2);
        sem_unlink(SEM_DAY_OVER);

        for (int i = 0; i < MAX_CUSTOMER_SEMS; ++i) {
            char sem_name[64];
            snprintf(sem_name, sizeof(sem_name), "%s%d", SEM_CUSTOMER_BASE, i);
            sem_unlink(sem_name);
        }

        for (int i = 0; i < MAX_OBSERVERS; i++) {
            char fifo_name[64];
            snprintf(fifo_name, sizeof(fifo_name), "%s%d", OBSERVER_FIFO_BASE, i);
            unlink(fifo_name);
        }
    }
    printf("Процесс %d: Очистка завершена\n", getpid());
}