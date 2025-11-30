#include "shop.h"

int main() {
    printf("Наблюдатель %d начал сбор информации...\n", getpid());
    printf("=====================================\n");
    
    setup_signal_handlers();

    if (init_shared_resources() == -1) {
        fprintf(stderr, "Ошибка инициализации ресурсов\n");
        return 1;
    }

    // Регистрируем этого наблюдателя
    if (sem_wait(mutex) == 0) {
        shop->active_processes++;
        sem_post(mutex);
    } else {
        perror("observer sem_wait");
    }
    register_observer(getpid());

    // Создаем свой именованный канал
    char fifo_name[64];
    snprintf(fifo_name, sizeof(fifo_name), "%s%d", OBSERVER_FIFO_BASE, getpid());
    mkfifo(fifo_name, 0666);

    // Открываем для чтения
    int fifo_fd = open(fifo_name, O_RDONLY | O_NONBLOCK);
    if (fifo_fd == -1) {
        perror("open observer fifo");
        cleanup_resources();
        return 1;
    }

    char buffer[512];
    fd_set read_fds;
    
    while (!shutdown_flag) {
        FD_ZERO(&read_fds);
        FD_SET(fifo_fd, &read_fds);
        
        struct timeval timeout = {1, 0}; // Таймаут 1 секунда
        
        int ready = select(fifo_fd + 1, &read_fds, NULL, NULL, &timeout);
        
        if (ready > 0 && FD_ISSET(fifo_fd, &read_fds)) {
            ssize_t bytes_read = read(fifo_fd, buffer, sizeof(buffer) - 1);
            if (bytes_read > 0) {
                buffer[bytes_read] = '\0';
                printf("%s", buffer);
                fflush(stdout);
            }
        }
        
        // Проверяем shutdown_flag
        if (shutdown_flag) {
            break;
        }
    }

    // Завершение работы
    close(fifo_fd);
    unlink(fifo_name);
    
    // Сначала отпишем наблюдателя
    unregister_observer(getpid());
    if (sem_wait(mutex) == 0) {
        shop->active_processes--;
        sem_post(mutex);
    } else {
        perror("observer sem_wait on exit");
    }
    
    cleanup_resources();
    printf("Наблюдатель %d завершил работу\n", getpid());
    return 0;
}