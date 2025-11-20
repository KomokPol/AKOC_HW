// message.h
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>

#define PERMS 0666

// Коды сообщений
#define MSG_TYPE_EMPTY  0     // Разделяемая память пуста
#define MSG_TYPE_NUMBER 1     // Передается число
#define MSG_TYPE_FINISH 2     // Сообщение о завершении

// Структура сообщения
typedef struct {
  int type;
  int number;  // случайное число
  pid_t server_pid; // pid сервера для отправки сигналов
  pid_t client_pid; // pid клиента для отправки сигналов
} message_t;

const char* shar_object = "random-numbers-shm";

// Функций для работы с сигналами
void setup_signal_handlers(void);
void handle_signal(int sig);
