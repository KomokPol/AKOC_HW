#include "shop.h"

void customer(shop_t *shop, int index) {
  srand(time(NULL) + getpid() + index * 1000); // Инициализируем генератор с уникальным значением

  // Покупатели завершаются через флаги в разделяемой памяти, а не напрямую по сигналам
  signal(SIGINT, SIG_IGN);
  signal(SIGTERM, SIG_IGN);

  // Генерируем список покупок
  int list_length = rand() % 5 + 1; // Длина списка от 1 до 5
  int shopping_list[5];
    
  // Генерируем отделы для покупок
  for (int i = 0; i < list_length; i++) {
    shopping_list[i] = rand() % 2 + 1;
  }
  
  printf("\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\nПокупатель %d: Мой список покупок: ", index);
  for (int i = 0; i < list_length; i++) {
    printf("%d ", shopping_list[i]);
  }
  printf("\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
    
  // Делаем покупки
  for (int i = 0; i < list_length; i++) {
    int department = shopping_list[i]; // Текущий отдел для покупки
        
    // Проверяем завершение рабочего дня
    sem_wait(&shop->mutex);
    if (shop->day_over) {
      sem_post(&shop->mutex);
      printf("Покупатель %d: Магазин закрывается, ухожу\n", index);
      break;
    }
    sem_post(&shop->mutex);
        
    // Встаем в очередь соответствующего отдела
    sem_wait(&shop->mutex);
    if (department == 1) {
      if ((shop->rear1 + 1) % MAX_QUEUE == shop->front1) { // Если очередь заполнена
        printf("Покупатель %d: Очередь 1 отдела заполнена, жду...\n", index);
        sem_post(&shop->mutex); 
        sleep(1); // Ждем 1 секунду
        i--; // Повторяем попытку для этого же товара
        continue;
      }
            
      // Добавляем покупателя в очередь 1 отдела
      shop->queue1[shop->rear1] = index;
      shop->rear1 = (shop->rear1 + 1) % MAX_QUEUE;
      sem_post(&shop->mutex);
            
      // Уведомляем продавца 1 о новом покупателе
      sem_post(&shop->seller1_sem);
      printf("Покупатель %d: Жду в очереди 1 отдела для товара %d\n", index, department);
    } else {
      if ((shop->rear2 + 1) % MAX_QUEUE == shop->front2) { // Если очередь заполнена
        printf("Покупатель %d: Очередь 2 отдела заполнена, жду...\n", index);
        sem_post(&shop->mutex);
        sleep(1);
        i--;
        continue;
      }
            
      // Добавляем покупателя в очередь 2 отдела
      shop->queue2[shop->rear2] = index;
      shop->rear2 = (shop->rear2 + 1) % MAX_QUEUE;
      sem_post(&shop->mutex);
            
      // Уведомляем продавца_2 о новом покупателе
      sem_post(&shop->seller2_sem);
      printf("Покупатель %d: Жду в очереди 2 отдела для товара %d\n", index, department);
    }
        
    // Ожидание обслуживания
    sem_wait(&shop->customer_sems[index]);

    // Проверяем, не было ли аварийного завершения
    sem_wait(&shop->mutex);
    int shutdown = shop->shutdown_flag || shop->day_over;
    sem_post(&shop->mutex);

    if (shutdown) {
      // Если магазин закрывается, уходим
      printf("Покупатель %d: Магазин закрывается, ухожу\n", index);
      break;
    }

    printf("Покупатель %d: Купил товар из отдела %d\n", index, department);
  }
    
  printf("\nПокупатель %d: Завершил покупки\n\n", index);
}
