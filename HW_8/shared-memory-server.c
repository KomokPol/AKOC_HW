// shared-memory-server.c
#include "message.h"

// Флаг завершения с гарантией, что каждое чтение переменной будет из памяти
volatile sig_atomic_t shutdown_flag = 0; 

// Обработчик сигналов
void handle_signal(int signal) {
  printf("\nСервер: получен сигнал %d, завершаем работу...\n", signal);
  shutdown_flag = 1;
}

// Настройка обработчиков сигналов
void setup_signal_handlers(void) {
  signal(SIGINT, handle_signal);   // Ctrl+C в термнале
  signal(SIGTERM, handle_signal);  // Команда kill в коде
}

void sys_err (char *msg) {
  puts (msg);
  exit (1);
}

int main () {
  int shmid;
  message_t *msg_p;

  // Настройка обработчиков сигналов
  setup_signal_handlers();
  
  // Создаем разделяемую память
  if ( (shmid = shm_open(shar_object, O_CREAT|O_RDWR, 0666)) == -1 ) {
    perror("shm_open");
    sys_err ("Сервер: объект уже открыт");
  } else {
    printf("Сервер: объект открыт: имя = %s, id = 0x%x\n", shar_object, shmid);
  }
  
  // Задаем размер объекта памяти
  if (ftruncate(shmid, sizeof (message_t)) == -1 ) {
    perror("ftruncate");
    sys_err ("Сервер: ошибка определения размера памяти");
    return 1;
  } else {
    printf("Сервер: объем памяти установлен и равен %lu\n", sizeof (message_t));
  }

  // Получаем доступ к памяти
  msg_p = mmap(0, sizeof (message_t), PROT_WRITE|PROT_READ, MAP_SHARED, shmid, 0);
  if (msg_p == (message_t*)-1 ) {
    perror("mmap");
    sys_err ("Сервер: неправильный доступ к памяти");
  }

  // Инициализируем
  msg_p->type = MSG_TYPE_EMPTY;
  msg_p->number = 0;
  msg_p->server_pid = getpid(); // Сохраняем pid сервера
  msg_p->client_pid = 0; // Пока клиент не подключился
  
  printf("Сервер запущен (pid: %d). Ждем ввод чисел...\n", getpid());
  printf("\nНажмите Ctrl+C, чтобы остановить работу сервера и клиента\n");

  int numbers_received = 0; // Обработанные числа
  
  while (shutdown_flag == 0) {
    // Если есть сообщение
    if (msg_p->type != MSG_TYPE_EMPTY) {
      // Если это число
      if (msg_p->type == MSG_TYPE_NUMBER) {
        numbers_received++;
        printf("Сервер получил число %d: %d\n", numbers_received, msg_p->number);
      } else if (msg_p->type == MSG_TYPE_FINISH) { // Иначе если финиш
        printf("Сервер: получен сигнал завершения от клиента\n");
        break;
      }
      msg_p->type = MSG_TYPE_EMPTY; // Сообщение обработано
    }
    usleep(100000); // Остановка на 100ms, чтобы цикл с сумасшедней скоростью не проходил миллионы раз за просто так
  }

  // Если сервер завершается по сигналу, уведомляем клиента
  if (shutdown_flag && msg_p->client_pid != 0) {
    printf("Сервер: уведомляет клиента (pid: %d) о завершении...\n", msg_p->client_pid);
    kill(msg_p->client_pid, SIGTERM); // Отправляем сигнал клиенту
    sleep(1); // Даем время клиенту на обработку
  }

  // Удаление разделяемой памяти
  printf("Сервер: удаление общей памяти...\n");
  if(shm_unlink(shar_object) == -1) {
    perror("shm_unlink");
    sys_err ("Сервер: ошибка при удалении общей памяти");
  } else {
    printf("Сервер: общая память успешно удалена\n");
  }

  printf("Сервер: отключен. Общее количество полученных сообщений: %d\n", numbers_received);
  return 0;
}
