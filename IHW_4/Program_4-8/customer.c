#include "shop.h"

// customer: поток-покупатель
// arg: указатель на int с индексом покупателя
void *customer(void *arg) {
  int customer_id = *((int*)arg); // ID покупателя
  shop_t *shop = global_shop; // Глобальный указатель на магазин

  // Генерируем уникальный локальный seed для rand_r, чтобы генерция была разной у потоков
  unsigned int seed = (unsigned)time(NULL) ^ (unsigned)customer_id ^ (unsigned)pthread_self();

  // Генерируем список покупок: длина 1-5
  int list_length = (rand_r(&seed) % 5) + 1;
  int shopping_list[5];

  for (int i = 0; i < list_length; i++) {
    shopping_list[i] = (rand_r(&seed) % 2) + 1; // Генерируем отдел
  }

  LOG("\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\nПокупатель %d: Мой список покупок: ", customer_id);
  for (int i = 0; i < list_length; i++) LOG("%d ", shopping_list[i]);
  LOG("\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");

  // Проходим по списку последовательно и покупаем товары в указанном порядке
  for (int i = 0; i < list_length; i++) {
    int department = shopping_list[i]; // Текущий отдел

    // Проверяем, закрыт ли магаз
    pthread_mutex_lock(&shop->mutex);
    if (shop->day_over) {
      pthread_mutex_unlock(&shop->mutex);
      LOG("Покупатель %d: Магазин закрывается, ухожу\n", customer_id);
      break;
    }
    pthread_mutex_unlock(&shop->mutex);

    // Встаем в очередь отдела под мьютексом
    pthread_mutex_lock(&shop->mutex);
    if (department == 1) {
      // Проверяем, есть ли место в очереди 1
      if ((shop->rear1 + 1) % MAX_QUEUE == shop->front1) {
        // Очередь полна, тогда отпускаем мьютекс и ждем, а потом повторяем попытк
        pthread_mutex_unlock(&shop->mutex);
        LOG("Покупатель %d: Очередь 1 заполнена, жду...\n", customer_id);
        sleep(1);
        i--; // Возобновляем попытку
        continue;
      }
      
      shop->queue1[shop->rear1] = customer_id; // Добавляем ID покупателя в очередь
      shop->rear1 = (shop->rear1 + 1) % MAX_QUEUE; // Сдвигаем указатель начала очереди
      pthread_mutex_unlock(&shop->mutex);

      sem_post(&shop->seller1_sem); // Уведомляем продавца 1
      LOG("Покупатель %d: Встал в очередь 1 для товара %d\n", customer_id, department);
    } else {
      // Аналогично для очереди 2
      if ((shop->rear2 + 1) % MAX_QUEUE == shop->front2) {
        pthread_mutex_unlock(&shop->mutex);
        LOG("Покупатель %d: Очередь 2 заполнена, жду...\n", customer_id);
        sleep(1);
        i--;
        continue;
      }
      shop->queue2[shop->rear2] = customer_id;
      shop->rear2 = (shop->rear2 + 1) % MAX_QUEUE;
      pthread_mutex_unlock(&shop->mutex);

      sem_post(&shop->seller2_sem);
      LOG("Покупатель %d: Встал в очередь 2 для товара %d\n", customer_id, department);
    }

    // Ожидаем обслуживания 
    sem_wait(&shop->customer_sems[customer_id - 1]);

    // После пробуждения проверяем, не было ли аварийного выхода
    pthread_mutex_lock(&shop->mutex);
    int shutdown = shop->shutdown_flag || shop->day_over;
    pthread_mutex_unlock(&shop->mutex);

    if (shutdown) {
      LOG("Покупатель %d: Магазин закрывается, ухожу\n", customer_id);
      break;
    }

    LOG("Покупатель %d: Купил товар из отдела %d\n", customer_id, department);
  }

  LOG("\nПокупатель %d: Завершил покупки\n\n", customer_id);
  return NULL;
}
