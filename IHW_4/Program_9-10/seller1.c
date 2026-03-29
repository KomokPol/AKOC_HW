#include "shop.h"

// seller1_alt: поток-продавец для отдела 1
// arg: указатель на shop_alt_t
void *seller1_alt(void *arg) {
    shop_alt_t *shop = (shop_alt_t*)arg; // Получаем указатель на структуру магазина
    
    LOGA("\nПродавец 1 начал работу\n");
    unsigned int seed = (unsigned)time(NULL) ^ (unsigned)(uintptr_t)pthread_self();
    while (1) {
        pthread_mutex_lock(&shop->mutex); // Защищаем доступ к очереди и флагам

        // Ждем появления клиента в очереди или флага shutdown; используем timedwait, чтобы периодически просыпаться
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts); // Получаем текущее время
        ts.tv_sec += 1; // 1 сек таймаут

        // Пока очередь пуста и рабочий день не закончен/shutdown
        while (shop->front1 == shop->rear1 && atomic_load(&shop->shutdown_flag) == 0 && !shop->day_over) {
            // Либо кто-то вызовет pthread_cond_signal, либо таймаут
            pthread_cond_timedwait(&shop->seller1_cond, &shop->mutex, &ts); // Ждем появления клиента
            clock_gettime(CLOCK_REALTIME, &ts); // Получаем текущее время
            ts.tv_sec += 1; // 1 сек таймаут
        }

        // Если рабочий день закончен/shutdown и очередь пуста, то завершаем
        if ((shop->day_over || atomic_load(&shop->shutdown_flag)) && shop->front1 == shop->rear1) {
            pthread_mutex_unlock(&shop->mutex);
            break;
        }

        // Если очередь пуста, но рабочий день не закончился, просто пропускаем мьютекс и ждем дальше
        if (shop->front1 == shop->rear1) {
            pthread_mutex_unlock(&shop->mutex);
            continue;
        }

        int customer_id = shop->queue1[shop->front1]; // Извлекаем покупателя из очереди
        shop->front1 = (shop->front1 + 1) % MAX_QUEUE; // Сдвигаем указатель начала очереди

        pthread_mutex_unlock(&shop->mutex); // Освобождаем мьютекс

        // Симулируем время обслуживания: 1-2 секунды, но используем thread_rand
        LOGA("Продавец 1: Обслуживает покупателя %d\n", customer_id);
        unsigned int r = thread_rand(&seed) % 2; /* 0..1 */
        sleep(1 + r);

        // Оповещаем конкретного покупателя через его условную переменную 
        pthread_mutex_lock(&shop->customer_mutexes[customer_id - 1]); // Защищаем доступ к флагу
        shop->customer_flags[customer_id - 1] = 1; // Устанавливаем флаг
        pthread_cond_signal(&shop->customer_conds[customer_id - 1]); // Оповещаем
        pthread_mutex_unlock(&shop->customer_mutexes[customer_id - 1]); // Освобождаем

        LOGA("Продавец 1: Закончил обслуживание покупателя %d\n", customer_id);
    }

    LOGA("Продавец 1 завершил работу\n");
    return NULL;
}
