#include "shop.h"

// seller2_alt: поток-продавец для отдела 2
// arg: указатель на shop_alt_t 
void *seller2_alt(void *arg) {
    shop_alt_t *shop = (shop_alt_t*)arg; // Получаем указатель на структуру магазина

    LOGA("Продавец 2 начал работу\n-----------------------\n"); /* Сообщение о старте */
    unsigned int seed = (unsigned)time(NULL) ^ (unsigned)(uintptr_t)pthread_self();

    while (1) {
        pthread_mutex_lock(&shop->mutex);

        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 1;

        while (shop->front2 == shop->rear2 && atomic_load(&shop->shutdown_flag) == 0 && !shop->day_over) {
            pthread_cond_timedwait(&shop->seller2_cond, &shop->mutex, &ts);
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += 1;
        }

        if ((shop->day_over || atomic_load(&shop->shutdown_flag)) && shop->front2 == shop->rear2) {
            pthread_mutex_unlock(&shop->mutex);
            break;
        }

        if (shop->front2 == shop->rear2) {
            pthread_mutex_unlock(&shop->mutex);
            continue;
        }

        int customer_id = shop->queue2[shop->front2];
        shop->front2 = (shop->front2 + 1) % MAX_QUEUE;

        pthread_mutex_unlock(&shop->mutex);

        LOGA("Продавец 2: Обслуживает покупателя %d\n", customer_id);
        unsigned int r = thread_rand(&seed) % 2;
        sleep(1 + r);

        pthread_mutex_lock(&shop->customer_mutexes[customer_id - 1]);
        shop->customer_flags[customer_id - 1] = 1;
        pthread_cond_signal(&shop->customer_conds[customer_id - 1]);
        pthread_mutex_unlock(&shop->customer_mutexes[customer_id - 1]);

        LOGA("Продавец 2: Закончил обслуживание покупателя %d\n", customer_id);
    }

    LOGA("Продавец 2 завершил работу\n");
    return NULL;
}
