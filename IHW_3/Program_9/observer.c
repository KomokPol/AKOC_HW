#include "shop.h"

int main() {
    printf("Наблюдатель начал сбор информации...\n");
    printf("=====================================\n");
    
    setup_signal_handlers();

    // Создаем/открываем FIFO для чтения
    mkfifo(OBSERVER_FIFO, 0666);
    int fifo_fd = open(OBSERVER_FIFO, O_RDONLY);
    if (fifo_fd == -1) {
        perror("open observer fifo for reading");
        return 1;
    }

    char buffer[512];
    ssize_t bytes_read;
    
    while (!shutdown_flag) {
        bytes_read = read(fifo_fd, buffer, sizeof(buffer) - 1);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            printf("%s", buffer); // Выводим полученное сообщение
            fflush(stdout);
        } else if (bytes_read == 0) {
            // Писатель закрыл соединение, ждем 1 сек
            sleep(1);
        } else {
            if (errno != EAGAIN) {
                perror("read from fifo");
                break;
            }
        }
    }

    // Завершение работы
    close(fifo_fd);
    unlink(OBSERVER_FIFO); // Удаляем FIFO
    printf("Наблюдатель завершил работу\n");

    return 0;
}