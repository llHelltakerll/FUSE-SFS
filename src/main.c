#include <stdlib.h>
#define FUSE_USE_VERSION 31
#include <asm-generic/errno-base.h>
#include <fuse3/fuse.h>
#include <stdio.h>
#include <string.h>

static int sfs_open(const char* path, struct fuse_file_info* fi)
{
    if (strcmp(path, "/hello") != 0) { return -ENOENT; }
    if ((fi->flags & O_ACCMODE) != O_RDONLY) { return -EACCES; }
    return 0;
}

static int sfs_read(const char* path, char* buf, size_t size, off_t offset,
                    struct fuse_file_info* fi)
{
    size_t len;
    (void)fi;
    printf("started\n");
    if (strcmp(path, "/hello") != 0) return -ENOENT;
    len = strlen("Hello World!");
    if (offset < len) {
        if (offset + size > len) size = len - offset;
        memcpy(buf, "Hello World!", size);
    }
    else
        size = 0;
    return size;
}

// вызывается при попытке прочитать содержимое каталога
static int sfs_readdir(
    const char* path,
    /*буфер в который будет записан результат чтения содержимого каталога*/
    void* buf,
    /*указатель на функцию которая будет использоватся для заполнения буфера
       данными*/
    fuse_fill_dir_t filler,
    /*смещение в каталоге*/ off_t offset, struct fuse_file_info* fi,
    /*флаги для чтения каталога*/ enum fuse_readdir_flags flags)
{
    if (strcmp(path, "/") != 0) { return -ENOENT; }

    // заполняем буфер информацией про каталог
    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);
    filler(buf, "hello", NULL, 0, 0);
    return 0;
}

// коллбэк который вызывается при попытке получить информацию о файле
static int sfs_gettatr(
    /*путь к файлу для которого требуется получить атребуты*/ const char* path,
    /*структура в которую будут записаны атрибуты файла*/ struct stat* stbuf,
    /*структура которая содаржит информацию о файле если он открыт*/
    struct fuse_file_info* fi)
{
    memset(stbuf, 0, sizeof(struct stat));
    if (strcmp(path, "/") == 0) { // если корневой каталог
        stbuf->st_mode = S_IFDIR | 0755; // установка прав
        stbuf->st_nlink = 2; // установка кол-ва жетских ссылок
    }
    else if (strcmp(path, "/hello") == 0) {
        stbuf->st_mode = S_IFREG | 0444; // только чтение
        stbuf->st_nlink
            = 1; // установка в 1, так как это обынчый файл без подкаталогов
        stbuf->st_size = strlen("hello world!");
    }
    else {
        return -ENOENT; // файл или каталог не существует
    }
    return 0; // усешно
}

// инициализация структуры коллбэками для операций с файловой системы
static struct fuse_operations sfs_opertations = {
    .getattr = sfs_gettatr,
    .readdir = sfs_readdir,
    .open = sfs_open, // вызывается при попытке открытия файла
    .read = sfs_read, // вызывается при попытке прочиатть данные из файла
};

int main(int argc, char* argv[])
{
    // инициализация структуры fuse_args с аргументами кс
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    // запуск файловой системы с заданными параметрами
    return fuse_main(args.argc, args.argv, &sfs_opertations, NULL);
}
