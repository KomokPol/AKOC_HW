// seller2.c
#include "shop.h"

int main() {
    setup_signal_handlers(); // Настраиваем обработчик сигналов

    if (init_shared_resources() == -1) { // Инициализируем разделяемые ресурсы
        fprintf(stderr, "Ошибка инициализации ресурсов\n");
        return 1;
    }

    sem_wait(mutex);
    shop->active_processes++; // Плюс процесс
    sem_post(mutex);

    srand(time(NULL) + getpid()); // Инициализация генератора случайных чисел
    
    printf("Продавец 2 начал работу\n------------------------\n");
    
    while (!shutdown_flag) {
        // Ожидаем покупателя с таймаутом
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts); // Получаем текущее время
        ts.tv_sec += 1; // Таймаут 1 секунда
        
        int wait_result = sem_timedwait(seller2_sem, &ts);
        
        if (wait_result == -1) {
            if (errno == ETIMEDOUT) { // Если время истекло
                continue; // Проверяем завершение
            } else {
                break;
            }
        }

        // Проверяем завершение
        sem_wait(mutex);
        if (shop->day_over || shutdown_flag) { // Если день закончился
            sem_post(mutex);
            break;
        }

        if (shop->front2 == shop->rear2) { // Если очередь пуста
            sem_post(mutex);
            continue;
        }

        // Извлекаем покупателя из очереди
        int customer_id = shop->queue2[shop->front2]; // Получаем индекс покупателя
        shop->front2 = (shop->front2 + 1) % MAX_QUEUE; // Циклический сдвиг очереди
        sem_post(mutex);
        
        // Обслуживаем покупателя
        printf("Продавец 2 обслуживает покупателя %d\n", customer_id);
        sleep(1 + rand() % 2); // Имитация времени обслуживания
                
        // Уведомляем покупателя
        sem_t *customer_sem = get_customer_sem(customer_id, 0);
        if (customer_sem) {
            sem_post(customer_sem); // Будим покупателя
            sem_close(customer_sem); // Закрываем дескриптор семафора
        }
        
        printf("Продавец 2 закончил обслуживание покупателя %d\n", customer_id);
    }

    printf("\n------------------------\nПродавец 2 завершил работу\n");
    cleanup_resources(); // Освобождение всех системных ресурсов
    return 0;
}