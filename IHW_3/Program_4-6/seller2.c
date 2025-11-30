// seller2.c
#include "shop.h"

void seller2(shop_t *shop) {
    // Продавец не реагирует на Ctrl+C напрямую, завершение управляется через флаги в разделяемой памяти
    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, SIG_IGN);

    printf("Продавец 2 начал работу\n-----------------------\n");
    srand(time(NULL) + getpid()); // Инициализация генератора случайных чисел
    
    while (1) {
        // Ожидаем покупателя
        sem_wait(&shop->seller2_sem);
        
        sem_wait(&shop->mutex);
        if (shop->day_over && shop->front2 == shop->rear2) {
            sem_post(&shop->mutex);
            break;
        }
        
        if (shop->front2 == shop->rear2) {
            sem_post(&shop->mutex);
            continue;
        }
        
        int customer_index = shop->queue2[shop->front2]; // Извлекаем покупателя из очереди
        shop->front2 = (shop->front2 + 1) % MAX_QUEUE; // Сдвигаем указатель начала очереди
        sem_post(&shop->mutex); // Освобождаем мьютекс
        
        // Обслуживаем покупателя
        printf("Продавец 2 обслуживает покупателя %d\n", customer_index);
        sleep(1 + rand() % 2); // Имитация времени обслуживания
        
        // Уведомляем покупателя об окончании обслуживания
        sem_post(&shop->customer_sems[customer_index]);
        printf("Продавец 2 закончил обслуживание покупателя %d\n", customer_index);
    }
    
    printf("Продавец 2 завершил работу\n");
}
