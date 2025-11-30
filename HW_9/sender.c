// sender.c
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>

volatile sig_atomic_t ack_received = 0; // Флаг подтверждения

// Обработчик подтверждения от приемника
void handle_ack(int sig) {
    (void)sig;
    ack_received = 1;
}

int main(void) {
    printf("Sender pid = %d\n", (int)getpid()); // Печатаем PID передатчика

    pid_t receiver_pid; // PID приемника
    printf("Input receiver PID: ");
    if (scanf("%d", &receiver_pid) != 1) { // Если ввод некорректен
        perror("scanf");
        return 1;
    }

    int number; // Вводимое число
    printf("Input decimal integer number: ");
    if (scanf("%d", &number) != 1) { // Если ввод некорректен
        perror("scanf");
        return 1;
    }

    signal(SIGUSR1, handle_ack); // Обработчик подтверждения

    unsigned int u = (unsigned int)number; // Преобразуем число в неотрицательное

    for (int i = 0; i < 32; ++i) { // Перебираем биты
        ack_received = 0; // Сбрасываем флаг
        int bit = (u >> i) & 1; // Получаем текущий бит
        int sig = bit ? SIGUSR2 : SIGUSR1; // Выбираем сигнал

        if (kill(receiver_pid, sig) == -1) { // Пытаемся отправить сигнал
            perror("kill");
            return 1;
        }

        putchar(bit ? '1' : '0'); // Печатаем бит
        fflush(stdout); // Обновляем вывод

        // Ждем, пока приемник не подтвердит
        while (!ack_received) {
            pause(); // Приостанавливаем выполнение
        }
    }

    putchar('\n');
    if (kill(receiver_pid, SIGINT) == -1) { // Отправляем сигнал
        perror("kill SIGINT");
        return 1;
    }

    printf("Result = %d\n", number);
    return 0;
}
