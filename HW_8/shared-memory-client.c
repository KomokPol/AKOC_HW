// shared-memory-client.c
#include "message.h"

// Флаг завершения с гарантией, что каждое чтение переменной будет из памяти
volatile sig_atomic_t shutdown_flag = 0;

// Обработчик сигналов
void handle_signal(int signal) {
  printf("\nКлиент: получен сигнал %d, инициирую завершение работы...\n", signal);
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
    sys_err ("Клиент: объект уже открыт");
  } else {
    printf("Клиент: объект открыт: имя = %s, id = 0x%x\n", shar_object, shmid);
  }

  // Получаем доступ к памяти
  msg_p = mmap(0, sizeof (message_t), PROT_WRITE|PROT_READ, MAP_SHARED, shmid, 0);
  if (msg_p == (message_t*)-1 ) {
    perror("mmap");
    sys_err ("Клиент: неправильный доступ к памяти");
  }

  // Сохраняем pid клиента в разделяемую память
  msg_p->client_pid = getpid();
  printf("Клиент запущен (pid: %d). Генерируем случайные числа...\n", getpid());
  printf("\nНажмите Ctrl+C, чтобы остановить работу клиента и сервера\n");

  srand(time(NULL)); // Инициализация генератора случайных чисел
  int numbers_sent = 0; // Отправленные числа
  
  while (shutdown_flag == 0) {
    // Пока сервер еще не обработал предыдущее число и не получен сигнал
    while (msg_p->type != MSG_TYPE_EMPTY && !shutdown_flag) {
      usleep(100000); // Ждем 100ms до следующего чтения
    }
    
    // Если клиент завершается по сигналу
    if (shutdown_flag) {
      break;
    }
    
    int random_num = rand() % 1000; // Число от 0 до 999
    numbers_sent++; 
    
    // Отправляем число
    msg_p->type = MSG_TYPE_NUMBER;
    msg_p->number = random_num;
    
    printf("Клиент отправил число %d: %d\n", numbers_sent, random_num);
    usleep(500000); // 500ms между числами
  }

  // Если клиент завершается по сигналу, сообщаем серверу
  if (shutdown_flag && msg_p->server_pid != 0) {
    printf("Клиент: уведомляет сервер (pid: %d) о завершении...\n", msg_p->server_pid);
    kill(msg_p->server_pid, SIGTERM); // Отправляем серверу сигнал
    
    // Также отправляем сообщение о завершении через разделяемую память
    while (msg_p->type != MSG_TYPE_EMPTY) {
      usleep(100000);
    }
    msg_p->type = MSG_TYPE_FINISH;
  }

  close(shmid);
  printf("Клиент: вышел. Общее количество отправленных сообщений: %d\n", numbers_sent);
  return 0;
}
