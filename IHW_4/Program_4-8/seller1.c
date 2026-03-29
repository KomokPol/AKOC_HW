// seller1.c
#include "shop.h"

// seller1: поток-продавец для отдела 1
// arg: указатель на shop_t
void *seller1(void *arg) {
  shop_t *shop = (shop_t*)arg; // Получаем указатель на структуру магазина

  LOG("\nПродавец 1 начал работу\n");
  unsigned int seed = (unsigned)time(NULL) ^ (unsigned)pthread_self();

  while (1) {
    sem_wait(&shop->seller1_sem); // Ждем сигнал о появлении покупателя
    pthread_mutex_lock(&shop->mutex); // Защищаем доступ к очереди и флагам

    // Если рабочий день закончился и очередь пуста, то завершаем работу
    if (shop->day_over && shop->front1 == shop->rear1) {
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

    // Симулируем время обслуживания: 1-2 секунды
    LOG("Продавец 1: Обслуживает покупателя %d\n", customer_id);
    sleep(1 + (rand_r(&seed) % 2));
    // Оповещаем покупателя о завершении обслуживания
    sem_post(&shop->customer_sems[customer_id - 1]);
    LOG("Продавец 1: Закончил обслуживание покупателя %d\n", customer_id);
  }

  LOG("Продавец 1 завершил работу\n");
  return NULL;
}
