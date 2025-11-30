// receiver.c
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>

volatile sig_atomic_t received = 0; // Принятые биты в порядке их прихода
volatile sig_atomic_t bit_count = 0; // Количество принятых бит
volatile sig_atomic_t done = 0; // Флаг завершения
pid_t transmitter_pid = 0; // PID отправителя

// Функция для корректного асинхронного вывода символа
void safe_write_char(char c) {
  write(1, &c, 1);
}

// Обработчик сигнала 1 (для 0)
void handle_sigusr1(int sig) {
    (void)sig;
    bit_count++; // Плюс принятый бит
    if (transmitter_pid) { // Если есть PID отправителя
      kill(transmitter_pid, SIGUSR1); // Отпраыляем подтверждение
    }
    safe_write_char('0'); // Печатаем 0
}

// Обработчик сигнала 2 (для 1)
void handle_sigusr2(int sig) {
    (void)sig;
    received |= (1u << bit_count); // Ставим 1 в соответствующий бит
    bit_count++; // Плюс принятый бит
    if (transmitter_pid) {
      kill(transmitter_pid, SIGUSR1);
    }
    safe_write_char('1');
}

// Обработчик SIGINT
void handle_sigint(int sig) {
    (void)sig;
    done = 1; // Ставим флаг завершения
}

int main(void) {
    printf("Receiver pid = %d\n", (int)getpid());

    printf("Input sender PID: ");
    if (scanf("%d", &transmitter_pid) != 1) {
      perror("scanf");
      return 1;
    }

    // Ставим обработчики
    signal(SIGUSR1, handle_sigusr1);
    signal(SIGUSR2, handle_sigusr2);
    signal(SIGINT, handle_sigint);

    // Ожидаем завершения
    while (!done) {
      pause();
    }

    putchar('\n');
    int final_value = (int)received; // Преобразуем полученные биты в число
    printf("Result = %d\n", final_value); // Печатаем результат
    return 0;
}
