#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

int buf_size = 32; // Разбер буфера для чтения/записи

int main(int argc, char* argv[]) {
  int fd_src = 0; // Дескриптор исходного файла
  int fd_dst = 0; // Дескриптор целевого файл
  
  struct stat src_stat; // Структура информации об исходном файле
  ssize_t read_bytes = 0; // Количество прочитанных байтов
  char buffer[buf_size]; // Буфер чтения/записи данных
  int rval = 0; // Для проверки прав доступа

  // Проверка аргументов командной строки
  if(argc != 3) {
    printf("Как нужно: %s исходный_файл целевой_файл\n", argv[0]);
    exit(-1);
  }

  // Открываем исходный файл только для чтения
  if((fd_src = open(argv[1], O_RDONLY)) < 0) {
    printf("Нельзя открыть исходный файл\n");
    exit(-1);
  }

  // Получаем информацию об исходном файле для прав доступа
  if(fstat(fd_src, &src_stat) < 0) {
    printf("Нельзя получить информацию об исходном файле\n");
    close(fd_src);
    exit(-1);
  }

  // Создаем файл с базовыми правами
  if((fd_dst = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0666)) < 0) {
    printf("Нельзя создать целевой файл\n");
    close(fd_src);
    exit(-1);
  }

  // Циклическое чтение и запись с буфером в 32 байта
  do {
    read_bytes = read(fd_src, buffer, buf_size);
    if(read_bytes == -1) {
      printf("Ошибка чтения из исходного файла\n");
      close(fd_src);
      close(fd_dst);
      exit(-1);
    }

    if(read_bytes > 0) {
      if(write(fd_dst, buffer, read_bytes) != read_bytes) {
        printf("Ошибка записи в целевой файл\n");
        close(fd_src);
        close(fd_dst);
        exit(-1);
      }
    }
  } while(read_bytes == buf_size); // Продолжаем, пока читаем полные блоки

  // Закрываем файлы
  if(close(fd_src) < 0) {
    printf("Ошибка закрытия исходного файла\n");
  }
  if(close(fd_dst) < 0) {
    printf("Ошибка закрытия целевого файла\n");
  }

  // Проверка, является ли исходный файл исполняемым
  rval = access(argv[1], X_OK);
  if (rval == 0) {
    // Если файл исполняемый, то устанавливаем оригинальные права доступа
    if(chmod(argv[2], src_stat.st_mode) < 0) {
      printf("Нельзя установить права доступа для исполняемого файла\n");
    } else {
      printf("Файл %s является исполняемым - права доступа сохранены\n", argv[1]);
    }
  } else {
    // Иначе если обычный текстовый файл, то убираем все биты исполнения
    mode_t text_mode = src_stat.st_mode & ~(S_IXUSR | S_IXGRP | S_IXOTH);
    if(chmod(argv[2], text_mode) < 0) {
      printf("Нельзя установить права доступа для текстового файла\n");
    } else {
      printf("Файл %s является текстовым - биты исполнения удалены\n", argv[1]);
    }
  }

  printf("Файл успешно скопирован с учетом прав доступа\n");
  return 0;
}