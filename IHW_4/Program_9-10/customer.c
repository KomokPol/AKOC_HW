#include "shop.h"

// customer_alt: поток-покупатель
// arg: указатель на int с индексом покупателя
void *customer_alt(void *arg) {
    int customer_id = *((int*)arg); // ID покупателя
    shop_alt_t *shop = global_shop_alt; // Глобальный указатель на магазин

    // Генерируем уникальный локальный seed для thread_rand 
    unsigned int seed = (unsigned)time(NULL) ^ (unsigned)customer_id ^ (unsigned)(uintptr_t)pthread_self();

    // Генерируем список покупок: длина 1-5
    int list_length = (thread_rand(&seed) % 5) + 1;
    int shopping_list[5];

    for (int i = 0; i < list_length; i++) {
        shopping_list[i] = (thread_rand(&seed) % 2) + 1; // Генерируем отдел
    }

    LOGA("\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\nПокупатель %d: Мой список покупок: ", customer_id);
    for (int i = 0; i < list_length; i++) LOGA("%d ", shopping_list[i]);
    LOGA("\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");

    // Проходим по списку последовательно и покупаем товары в указанном порядке
    for (int i = 0; i < list_length; i++) {
        int department = shopping_list[i]; // Текущий отдел

        // Проверяем, закрыт ли магазин
        pthread_mutex_lock(&shop->mutex);
        if (shop->day_over || atomic_load(&shop->shutdown_flag)) {
            pthread_mutex_unlock(&shop->mutex);
            LOGA("Покупатель %d: Магазин закрывается, ухожу\n", customer_id);
            break;
        }
        pthread_mutex_unlock(&shop->mutex);

        // Встаем в очередь соответствующего отдела
        pthread_mutex_lock(&shop->mutex);
        if (department == 1) {
            // Проверяем, есть ли место в очереди 1
            if ((shop->rear1 + 1) % MAX_QUEUE == shop->front1) {
                // Очередь полна, тогда отпускаем мьютекс и ждем, а потом повторяем попытк
                pthread_mutex_unlock(&shop->mutex);
                LOGA("Покупатель %d: Очередь 1 заполнена, жду...\n", customer_id);
                sleep(1);
                i--; // Возобновляем попытку
                continue;
            }
            shop->queue1[shop->rear1] = customer_id; // Добавляем ID покупателя в очередь
            shop->rear1 = (shop->rear1 + 1) % MAX_QUEUE; // Сдвигаем указатель начала очереди
      
            // Сигнализируем продавцу 1, что появился новый покупатель
            pthread_cond_signal(&shop->seller1_cond);
            pthread_mutex_unlock(&shop->mutex);
            LOGA("Покупатель %d: Встал в очередь 1 для товара %d\n", customer_id, department);
        } else {
            // Аналогично для очереди 2
            if ((shop->rear2 + 1) % MAX_QUEUE == shop->front2) {
                pthread_mutex_unlock(&shop->mutex);
                LOGA("Покупатель %d: Очередь 2 заполнена, жду...\n", customer_id);
                sleep(1);
                i--;
                continue;
            }
            shop->queue2[shop->rear2] = customer_id;
            shop->rear2 = (shop->rear2 + 1) % MAX_QUEUE;

            pthread_cond_signal(&shop->seller2_cond);
            pthread_mutex_unlock(&shop->mutex);
            LOGA("Покупатель %d: Встал в очередь 2 для товара %d\n", customer_id, department);
        }

        // Ждем своей условной переменной (с таймой, чтобы реагировать на shutdown)
        pthread_mutex_lock(&shop->customer_mutexes[customer_id - 1]);
        while (shop->customer_flags[customer_id - 1] == 0 && atomic_load(&shop->shutdown_flag) == 0 && !shop->day_over) {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += 1;
            pthread_cond_timedwait(&shop->customer_conds[customer_id - 1], &shop->customer_mutexes[customer_id - 1], &ts);
        }
        shop->customer_flags[customer_id - 1] = 0; // Сбрасываем флаг
        pthread_mutex_unlock(&shop->customer_mutexes[customer_id - 1]); // Отпускаем мьютекс

        // Проверяем, закрыт ли магазин
        if (atomic_load(&shop->shutdown_flag) || shop->day_over) {
            LOGA("Покупатель %d: Магазин закрывается, ухожу\n", customer_id);
            break;
        }

        LOGA("Покупатель %d: Купил товар из отдела %d\n", customer_id, department);
    }

    LOGA("\nПокупатель %d: Завершил покупки\n\n", customer_id);
    return NULL;
}
