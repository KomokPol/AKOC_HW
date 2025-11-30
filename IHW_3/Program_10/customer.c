#include "shop.h"

int main() {
    setup_signal_handlers();

    if (init_shared_resources() == -1) {
        fprintf(stderr, "Ошибка инициализации ресурсов\n");
        return 1;
    }

    sem_wait(mutex);
    int customer_id = shop->next_customer_id++;
    shop->active_processes++;
    sem_post(mutex);

    printf("Покупатель %d начал покупки\n", customer_id);
    send_to_observers("Начал покупки", "CUSTOMER", getpid());

    sem_t *my_sem = get_customer_sem(customer_id, 1);
    if (!my_sem) {
        fprintf(stderr, "Не удалось создать семафор покупателя\n");
        cleanup_resources();
        return 1;
    }

    srand(time(NULL) + getpid() + customer_id);

    // Генерируем список покупок
    int list_length = rand() % 5 + 1;
    int shopping_list[5];
    
    for (int i = 0; i < list_length; i++) {
        shopping_list[i] = rand() % 2 + 1;
    }
    
    printf("\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\nПокупатель %d: Мой список покупок: ", customer_id);
    for (int i = 0; i < list_length; i++) {
        printf("%d ", shopping_list[i]);
    }
    printf("\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
    
    char msg[200];
    snprintf(msg, sizeof(msg), "Список покупок: %d товаров", list_length);
    send_to_observers(msg, "CUSTOMER", getpid());
    
    // Выполняем покупки
    for (int i = 0; i < list_length && !shutdown_flag; i++) {
        int department = shopping_list[i];
        
        // Проверяем завершение
        sem_wait(mutex);
        if (shop->day_over || shutdown_flag) {
            sem_post(mutex);
            break;
        }
        sem_post(mutex);
        
        // Встаем в очередь
        sem_wait(mutex);
        if (department == 1) {
            if ((shop->rear1 + 1) % MAX_QUEUE == shop->front1) {
                printf("Покупатель %d: Очередь 1 отдела заполнена, жду...\n", customer_id);
                sem_post(mutex);
                sleep(1);
                i--; // Повторяем попытку
                continue;
            }
            
            shop->queue1[shop->rear1] = customer_id;
            shop->rear1 = (shop->rear1 + 1) % MAX_QUEUE;
            sem_post(mutex);
            
            sem_post(seller1_sem);
            printf("Покупатель %d: Встал в очередь 1 отдела для товара %d\n", customer_id, department);
            snprintf(msg, sizeof(msg), "Встал в очередь 1 отдела для товара %d", department);
            send_to_observers(msg, "CUSTOMER", getpid());
        } else {
            if ((shop->rear2 + 1) % MAX_QUEUE == shop->front2) {
                printf("Покупатель %d: Очередь 2 отдела заполнена, жду...\n", customer_id);
                sem_post(mutex);
                sleep(1);
                i--;
                continue;
            }
            
            shop->queue2[shop->rear2] = customer_id;
            shop->rear2 = (shop->rear2 + 1) % MAX_QUEUE;
            sem_post(mutex);
            
            sem_post(seller2_sem);
            printf("Покупатель %d: Встал в очередь 2 отдела для товара %d\n", customer_id, department);
            snprintf(msg, sizeof(msg), "Встал в очередь 2 отдела для товара %d", department);
            send_to_observers(msg, "CUSTOMER", getpid());
        }
        
        // Ожидаем обслуживания с таймаутом
        int served = 0;
        while (!served && !shutdown_flag) {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += 2; // Таймаут 2 секунды
            
            int wait_result = sem_timedwait(my_sem, &ts);
            if (wait_result == 0) {
                served = 1;
                printf("Покупатель %d: Купил товар из отдела %d\n", customer_id, department);
                snprintf(msg, sizeof(msg), "Купил товар из отдела %d", department);
                send_to_observers(msg, "CUSTOMER", getpid());
            } else if (errno != ETIMEDOUT) {
                perror("sem_timedwait customer");
                break;
            }
            
            // Проверяем завершение
            sem_wait(mutex);
            if (shop->day_over || shutdown_flag) {
                sem_post(mutex);
                break;
            }
            sem_post(mutex);
        }
        
        if (!served) {
            printf("Покупатель %d: Не удалось купить товар %d\n", customer_id, department);
        }
    }
    
    printf("\nПокупатель %d: Завершил покупки\n\n", customer_id);
    send_to_observers("Завершил покупки", "CUSTOMER", getpid());
    sem_close(my_sem);
    cleanup_resources();
    return 0;
}