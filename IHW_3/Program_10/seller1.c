// seller1.c
#include "shop.h"

int main() {
    setup_signal_handlers();

    if (init_shared_resources() == -1) {
        fprintf(stderr, "Ошибка инициализации ресурсов\n");
        return 1;
    }

    sem_wait(mutex);
    if (shop->active_processes == 0 || !shop->initialized) {
        shop->front1 = shop->rear1 = 0;
        shop->front2 = shop->rear2 = 0;
        shop->day_over = 0;
        shop->next_customer_id = 0;
        shop->initialized = 1;
        printf("Продавец 1: Инициализировал магазин\n");
        send_to_observers("Инициализировал магазин", "SELLER1", getpid());
    }
    shop->active_processes++;
    sem_post(mutex);

    srand(time(NULL) + getpid());
    
    printf("Продавец 1 начал работу\n------------------------\n");
    send_to_observers("Начал работу", "SELLER1", getpid());
    
    while (!shutdown_flag) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 1;
        
        int wait_result = sem_timedwait(seller1_sem, &ts);
        
        if (wait_result == -1) {
            if (errno == ETIMEDOUT) {
                continue;
            } else {
                perror("sem_timedwait seller1");
                break;
            }
        }

        sem_wait(mutex);
        if (shop->day_over || shutdown_flag) {
            sem_post(mutex);
            break;
        }

        if (shop->front1 == shop->rear1) {
            sem_post(mutex);
            continue;
        }

        int customer_id = shop->queue1[shop->front1];
        shop->front1 = (shop->front1 + 1) % MAX_QUEUE;
        sem_post(mutex);
        
        printf("Продавец 1 обслуживает покупателя %d\n", customer_id);
        char msg[100];
        snprintf(msg, sizeof(msg), "Обслуживает покупателя %d", customer_id);
        send_to_observers(msg, "SELLER1", getpid());
        
        sleep(1 + rand() % 2); // Имитация времени обслуживания
        
        // Уведомляем покупателя
        sem_t *customer_sem = get_customer_sem(customer_id, 0);
        if (customer_sem) {
            sem_post(customer_sem);
            sem_close(customer_sem);
        }
        
        printf("Продавец 1 закончил обслуживание покупателя %d\n", customer_id);
        // Отправляем сообщение наблюдателю
        snprintf(msg, sizeof(msg), "Закончил обслуживание покупателя %d", customer_id);
        send_to_observers(msg, "SELLER1", getpid());
    }

    printf("\n------------------------\nПродавец 1 завершил работу\n");
    send_to_observers("Завершил работу", "SELLER1", getpid());
    cleanup_resources();
    return 0;
}