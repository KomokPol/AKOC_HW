#include "shop.h"

// seller2: поток-продавец для отдела 2
// arg: указатель на shop_t
void *seller2(void *arg) {
  shop_t *shop = (shop_t*)arg; // Получаем указатель на структуру магазина

  LOG("Продавец 2 начал работу\n-----------------------\n");
  unsigned int seed = (unsigned)time(NULL) ^ (unsigned)pthread_self();

  while (1) {
    sem_wait(&shop->seller2_sem); // Ждем сигнал о появлении покупателя
    pthread_mutex_lock(&shop->mutex); // Защищаем доступ к очереди и флагам

    // Если рабочий день закончился и очередь пуста, то завершаем работу
    if (shop->day_over && shop->front2 == shop->rear2) {
      pthread_mutex_unlock(&shop->mutex);
      break;
    }

    // Если очередь пуста, но рабочий день не закончился, просто пропускаем мьютекс и ждем дальше
    if (shop->front2 == shop->rear2) {
      pthread_mutex_unlock(&shop->mutex);
      continue;
    }

    int customer_id = shop->queue2[shop->front2];
    shop->front2 = (shop->front2 + 1) % MAX_QUEUE;
    pthread_mutex_unlock(&shop->mutex);

    LOG("Продавец 2: Обслуживает покупателя %d\n", customer_id);
    sleep(1 + (rand_r(&seed) % 2));
    sem_post(&shop->customer_sems[customer_id - 1]);
    LOG("Продавец 2: Закончил обслуживание покупателя %d\n", customer_id);
  }

  LOG("Продавец 2 завершил работу\n");
  return NULL;
}
