// seller2.c
#include "shop.h"

int main() {
    setup_signal_handlers(); // Настраиваем обработчик сигналов
    init_observer_fifo(); // Инициализируем FIFO

    if (init_shared_resources() == -1) { // Инициализируем разделяемые ресурсы
        fprintf(stderr, "Ошибка инициализации ресурсов\n");
        return 1;
    }

    sem_wait(mutex);
    shop->active_processes++; // Плюс процесс
    sem_post(mutex);

    srand(time(NULL) + getpid()); // Инициализация генератора случайных чисел
    
    printf("Продавец 2 начал работу\n------------------------\n");
    send_to_observer("Начал работу", "SELLER2", getpid()); // Отправляем в FIFO
    
    while (!shutdown_flag) {
        // Ожидаем покупателя с таймаутом
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 1;
        
        int wait_result = sem_timedwait(seller2_sem, &ts);
        
        if (wait_result == -1) {
            if (errno == ETIMEDOUT) {
                continue;
            } else if (errno == EINTR) {
                if (shutdown_flag) break;
                continue;
            } else {
                perror("sem_timedwait seller2");
                break;
            }
        }

        // Проверяем завершение
        sem_wait(mutex);
        if (shop->day_over || shutdown_flag) {
            sem_post(mutex);
            break;
        }

        if (shop->front2 == shop->rear2) {
            sem_post(mutex);
            continue;
        }

        // Извлекаем покупателя из очереди
        int customer_id = shop->queue2[shop->front2];
        shop->front2 = (shop->front2 + 1) % MAX_QUEUE;
        sem_post(mutex);
        
        // Обслуживаем покупателя
        printf("Продавец 2 обслуживает покупателя %d\n", customer_id);
        // Пишем в FIFO
        char msg[100];
        snprintf(msg, sizeof(msg), "Обслуживает покупателя %d", customer_id);
        send_to_observer(msg, "SELLER2", getpid());
        sleep(1 + rand() % 2); // Имитация времени обслуживания
        
        // Уведомляем покупателя
        sem_t *customer_sem = get_customer_sem(customer_id, 0);
        if (customer_sem) {
            sem_post(customer_sem); // Будим покупателя
            sem_close(customer_sem); // Закрываем дескриптор семафора
        }
        
        printf("Продавец 2 закончил обслуживание покупателя %d\n", customer_id);
        snprintf(msg, sizeof(msg), "Закончил обслуживание покупателя %d", customer_id);
        send_to_observer(msg, "SELLER2", getpid());
    }

    printf("\n------------------------\nПродавец 2 завершил работу\n");
    send_to_observer("Завершил работу", "SELLER2", getpid());
    cleanup_resources();
    return 0;
}