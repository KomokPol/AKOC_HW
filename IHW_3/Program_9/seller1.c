// seller1.c
#include "shop.h"

int main() {
    setup_signal_handlers(); // Настраиваем обработчик сигналов
    init_observer_fifo(); // Инициализируем FIFO

    if (init_shared_resources() == -1) { // Инициализируем разделяемые ресурсы
        fprintf(stderr, "Ошибка инициализации ресурсов\n");
        return 1;
    }

    sem_wait(mutex);
    if (shop->active_processes == 0) {
        // Первый процесс инициализирует структуры данных магазина
        shop->front1 = shop->rear1 = 0;
        shop->front2 = shop->rear2 = 0;
        shop->day_over = 0;
        shop->next_customer_id = 0;
        printf("Продавец 1: Инициализировал магазин\n");
        send_to_observer("Инициализировал магазин", "SELLER1", getpid());
    }
    shop->active_processes++; // Плюс процесс
    sem_post(mutex);

    srand(time(NULL) + getpid()); // Инициализация генератора случайных чисел
    
    printf("Продавец 1 начал работу\n------------------------\n");
    send_to_observer("Начал работу", "SELLER1", getpid());
    
    while (!shutdown_flag) {
        // Ожидаем покупателя с таймаутом
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts); // Получаем текущее время
        ts.tv_sec += 1; // Таймаут 1 секунда
        
        int wait_result = sem_timedwait(seller1_sem, &ts); // Ожидаем
        
        if (wait_result == -1) {
            if (errno == ETIMEDOUT) { // Если время истекло
                continue; 
            } else if (errno == EINTR) {
                if (shutdown_flag) break;
                continue;
            } else {
                perror("sem_timedwait seller1");
                break;
            }
        }

        // Проверяем завершение
        sem_wait(mutex);
        if (shop->day_over || shutdown_flag) { // Если день закончился или завершение
            sem_post(mutex);
            break;
        }

        if (shop->front1 == shop->rear1) { // Если очередь пуста
            sem_post(mutex);
            continue;
        }

        // Извлекаем покупателя из очереди
        int customer_id = shop->queue1[shop->front1]; // Получаем индекс покупателя
        shop->front1 = (shop->front1 + 1) % MAX_QUEUE; // Циклический сдвиг очереди
        sem_post(mutex);
        
        // Обслуживаем покупателя
        printf("Продавец 1 обслуживает покупателя %d\n", customer_id);
        
        // Отправляем сообщение наблюдателю
        char msg[100];
        snprintf(msg, sizeof(msg), "Обслуживает покупателя %d", customer_id);
        send_to_observer(msg, "SELLER1", getpid());
        
        sleep(1 + rand() % 2); // Имитация времени обслуживания
        
        // Уведомляем покупателя об окончании обслуживания
        sem_t *customer_sem = get_customer_sem(customer_id, 0);
        if (customer_sem) {
            sem_post(customer_sem); // Будим покупателя
            sem_close(customer_sem); // Закрываем дескриптор семафора
        }
        
        printf("Продавец 1 закончил обслуживание покупателя %d\n", customer_id);
        // Отправляем сообщение наблюдателю
        snprintf(msg, sizeof(msg), "Закончил обслуживание покупателя %d", customer_id);
        send_to_observer(msg, "SELLER1", getpid());
    }

    printf("\n------------------------\nПродавец 1 завершил работу\n");
    send_to_observer("Завершил работу", "SELLER1", getpid());
    cleanup_resources(); // Освобождение всех системных ресурсов
    return 0;
}