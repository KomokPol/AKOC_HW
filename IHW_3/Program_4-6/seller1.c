// seller1.c
#include "shop.h"

void seller1(shop_t *shop) {
    // Продавец не реагирует на Ctrl+C напрямую, завершение управляется через флаги в разделяемой памяти
    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, SIG_IGN);

    printf("\nПродавец 1 начал работу\n");
    srand(time(NULL) + getpid()); // Инициализация генератора случайных чисел
    
    while (1) {
        // Ожидаем покупателя
        sem_wait(&shop->seller1_sem);
        
        sem_wait(&shop->mutex);
        if (shop->day_over && shop->front1 == shop->rear1) { // Если рабочий день закончился и очередь пуста
            sem_post(&shop->mutex);
            break;
        }
        
        if (shop->front1 == shop->rear1) { // Если очередь пуста, но рабочий день не закончился
            sem_post(&shop->mutex);
            continue;
        }

        // Извлекаем покупателя из очереди
        int customer_index = shop->queue1[shop->front1]; // Получаем индекс покупателя
        shop->front1 = (shop->front1 + 1) % MAX_QUEUE; // Сдвигаем указатель начала очереди
        sem_post(&shop->mutex);
        
        // Обслуживаем покупателя
        printf("Продавец 1 обслуживает покупателя %d\n", customer_index);
        sleep(1 + rand() % 2); // Имитация времени обслуживания
        
        // Уведомляем покупателя об окончании обслуживания
        sem_post(&shop->customer_sems[customer_index]);
        printf("Продавец 1 закончил обслуживание покупателя %d\n", customer_index);
    }
    
    printf("Продавец 1 завершил работу\n");
}
