#include <asm-generic/errno-base.h>
#define FUSE_USE_VERSION  30

#define _FILE_OFFSET_BITS 64
#include "bitmap.h"
#include <errno.h>
#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define BLOCKS_PER_INODE 10

typedef struct inode {
    mode_t mode;
    int size_blocks;
    time_t data_access;
    time_t data_change;
    time_t data_modification;
    size_t n_links;
    uid_t uid;
    gid_t gid;
    size_t len;
    int blocks[BLOCKS_PER_INODE];
} inode_t;

#define MAX_INODES 500
static inode_t INODES[MAX_INODES];
static bitmap_t inode_bitmap;

typedef struct dirent {
    char name[50];
    int inode_number;
} dirent_t;

#define MAX_BLOCKS   500
#define MAX_DIRENTS  50

#define MAX_PATH_LEN 500
#define MAX_NAME_LEN 50

static size_t BLOCK_SIZE = sizeof(dirent_t) * MAX_DIRENTS;

static bitmap_t block_bitmap;
static char BLOCKS[MAX_BLOCKS][sizeof(dirent_t) * MAX_DIRENTS];

void parse_path(const char* path, char* directory, char* filename)
{
    const char* last_slash = strrchr(path, '/');
    const char* last_backslash = strrchr(path, '\\');
    const char* filename_start;

    if (last_slash == NULL && last_backslash == NULL) { filename_start = path; }
    else {
        const char* last_separator
            = (last_slash > last_backslash) ? last_slash : last_backslash;
        filename_start = last_separator + 1;
    }

    if (directory != NULL) {
        size_t directory_length = filename_start - path;
        strncpy(directory, path, directory_length);
        directory[directory_length] = '\0';
    }

    if (filename != NULL) { strcpy(filename, filename_start); }
}

int add_dir_entry(const char* filename, int dir_inode, int new_inode)
{
    // if inode.mode != DIR {
    // 	return -1;
    // }
    inode_t* inode = &INODES[dir_inode];

    dirent_t root;
    strcpy(root.name, filename);
    root.inode_number = new_inode;
    memcpy(BLOCKS[inode->blocks[0]] + (inode->len * sizeof(dirent_t)), &root,
           sizeof(dirent_t));

    inode->len = inode->len + 1;

    return 0;
}

int remove_inode(int inode_index, dirent_t* directory)
{
    inode_t* inode = &INODES[inode_index];

    if ((inode->mode & __S_IFDIR) != 0) {
        if (inode->len > 2) return -ENOTEMPTY;
    }
    else {
        if (inode->n_links > 1) {
            inode->n_links -= 1;
            return 0;
        }
    }

    remove_elements(&inode_bitmap, inode_index, 1);
    for (size_t i = 0; i < inode->size_blocks; i++) {
        remove_elements(&block_bitmap, inode->blocks[i], 1);
    }

    return 0;
}

int remove_dir_entry(const char* filename, int inode_index)
{
    // if inode.mode != DIR {
    // 	return -1;
    // }
    inode_t* parent_inode = &INODES[inode_index];
    int block_index = parent_inode->blocks[0];

    dirent_t* directory = (dirent_t*)&BLOCKS[block_index];
    for (size_t i = 0; i < INODES[inode_index].len; i++) {
        if (strcmp(directory->name, filename) == 0) {
            int res = remove_inode(directory->inode_number, directory);
            if (res == 0) {
                dirent_t* ultimo = (dirent_t*)&BLOCKS[block_index];
                ultimo += parent_inode->len - 1;
                memcpy(directory, ultimo, sizeof(dirent_t));
                memset(ultimo, 0, sizeof(dirent_t));

                parent_inode->len = parent_inode->len - 1;
            }

            return res;
        }
        directory++;
    }

    return -1;
}

int get_inode_index(const char* filename, int inode_index)
{
    int block_index = INODES[inode_index].blocks[0];

    dirent_t* directory = (dirent_t*)&BLOCKS[block_index];
    for (size_t i = 0; i < INODES[inode_index].len; i++) {
        if (strcmp(directory->name, filename) == 0)
            return directory->inode_number;
        directory++;
    }

    return -1;
}

int get_inode(const char* _path)
{
    if (strcmp(_path, "/") == 0) return 0;

    char* path = malloc(strlen(_path) * sizeof(char));
    strcpy(path, _path);

    char* delimiter = "/";
    char* token = strtok(path, delimiter);

    int parent_index_inode = 0;
    while (token != NULL) {
        int index = get_inode_index(token, parent_index_inode);

        if (index < 0) {
            free(path);
            return -1;
        }
        else {
            parent_index_inode = index;
        }

        token = strtok(NULL, delimiter);
    }

    free(path);
    return parent_index_inode;
}

int fisopfs_mkdir(const char* path, mode_t mode)
{
    printf("[debug] fisopfs_mkdir - path: %s\n", path);

    char directory[MAX_PATH_LEN];
    char name[MAX_NAME_LEN];
    parse_path(path, directory, name);

    int parent_index_inode = get_inode(directory);
    if (parent_index_inode < -1) return -ENOENT;

    int inode_index = allocate_elements(&inode_bitmap, 1);
    if (inode_index < 0) return -ENOMEM;

    int block_index = allocate_elements(&block_bitmap, 1);
    if (block_index < 0) {
        remove_elements(&inode_bitmap, inode_index, 1);
        return -ENOMEM;
    }

    inode_t* inode = &INODES[inode_index];
    inode->mode = __S_IFDIR | mode;
    inode->size_blocks = 1;
    inode->blocks[0] = block_index;
    inode->len = 2;
    inode->n_links = 1;
    inode->data_change = time(NULL);
    inode->data_access = time(NULL);
    inode->data_modification = time(NULL);
    inode->uid = getuid();
    inode->gid = getgid();

    dirent_t new_block[2] = {{".", inode_index}, {"..", parent_index_inode}};
    memcpy(BLOCKS[block_index], new_block, 2 * sizeof(dirent_t));

    add_dir_entry(name, parent_index_inode, inode_index);

    return 0;
}

int fisopfs_getattr(const char* path, struct stat* st)
{
    printf("[debug] fisopfs_getattr - path: %s\n", path);

    int inode_index = get_inode(path);
    if (inode_index < 0) return -ENOENT;

    inode_t* inode = &INODES[inode_index];
    st->st_uid = inode->uid;
    st->st_mode = INODES[inode_index].mode;
    st->st_size = inode->len;
    st->st_nlink = inode->n_links;
    st->st_ino = inode_index;

    st->st_atime = inode->data_access;
    st->st_mtime = inode->data_modification;
    st->st_ctime = inode->data_change;

    return 0;
}

void fisopfs_destroy(void* private_data)
{
    char cwd[1024];
    char filepath[2048];

    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        strcpy(filepath, cwd);
        strcat(filepath, "/fs.fisopfs");
    }

    int fd = open(filepath, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fd < 0) { return; }

    write(fd, INODES, MAX_INODES * sizeof(inode_t));
    write(fd, BLOCKS, MAX_BLOCKS * BLOCK_SIZE);
    write(fd, inode_bitmap.bits,
          ((inode_bitmap.size + 31) / 32) * sizeof(unsigned int));
    write(fd, block_bitmap.bits,
          ((block_bitmap.size + 31) / 32) * sizeof(unsigned int));

    close(fd);

    destroy_bitmap(&inode_bitmap);
    destroy_bitmap(&block_bitmap);
    return;
}

void* fisopfs_init(struct fuse_conn_info* conn)
{
    char cwd[1024];
    char filepath[2048];

    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        strcpy(filepath, cwd);
        strcat(filepath, "/fs.fisopfs");
    }

    create_bitmap(&inode_bitmap, MAX_INODES);
    create_bitmap(&block_bitmap, MAX_BLOCKS);

    int fd = open(filepath, O_RDONLY, S_IRUSR | S_IWUSR);
    if (fd > 0) {
        read(fd, INODES, MAX_INODES * sizeof(inode_t));
        read(fd, BLOCKS, MAX_BLOCKS * BLOCK_SIZE);

        read(fd, inode_bitmap.bits,
             ((inode_bitmap.size + 31) / 32) * sizeof(unsigned int));

        read(fd, block_bitmap.bits,
             ((block_bitmap.size + 31) / 32) * sizeof(unsigned int));

        close(fd);

        return 0;
    }

    memset(BLOCKS, 0, BLOCK_SIZE * MAX_BLOCKS);
    memset(INODES, 0, sizeof(inode_t) * MAX_INODES);

    int inode_index = allocate_elements(&inode_bitmap, 1);
    int block_index = allocate_elements(&block_bitmap, 1);

    inode_t* inode = &INODES[inode_index];
    inode->mode = __S_IFDIR | 0755;
    inode->size_blocks = 1;
    inode->blocks[0] = block_index;
    inode->len = 2;

    dirent_t new_block[2] = {{".", 0}, {"..", 0}};
    memcpy(BLOCKS[block_index], new_block, 2 * sizeof(dirent_t));

    return 0;
}

int fisopfs_readdir(const char* path, void* buffer, fuse_fill_dir_t filler,
                    off_t offset, struct fuse_file_info* fi)
{
    printf("[debug] fisopfs_readdir - path: %s\n", path);

    int inode_index = get_inode(path);
    if (inode_index < 0) return -ENOENT;

    inode_t* inode = &INODES[inode_index];

    struct stat st;
    st.st_uid = inode->uid;
    st.st_mode = inode->mode;
    st.st_size = inode->len;
    st.st_nlink = inode->n_links;

    int block_index = INODES[inode_index].blocks[0];

    dirent_t* directory = (dirent_t*)&BLOCKS[block_index];
    for (size_t i = 0; i < INODES[inode_index].len; i++) {
        filler(buffer, directory->name, &st, 0);
        directory++;
    }

    return 0;
}

int fisopfs_read(const char* path, char* buffer, size_t size, off_t offset,
                 struct fuse_file_info* fi)
{
    printf("[debug] fisopfs_read - path: %s, offset: %lu, size: %lu\n", path,
           offset, size);

    inode_t* inode = &INODES[fi->fh];
    if (size > inode->len) size = inode->len;

    size_t offset_blocks = offset / BLOCK_SIZE;
    if (offset_blocks > inode->size_blocks) {
        printf("SEGMENTATION FAULT\n");
        return -1;
    }

    size_t offset_size = offset % BLOCK_SIZE;
    if (BLOCK_SIZE > offset_size + size) {
        memcpy(buffer, BLOCKS[inode->blocks[offset_blocks]] + offset_size,
               size);
        return size;
    }

    memcpy(buffer, BLOCKS[inode->blocks[offset_blocks]] + offset_size,
           BLOCK_SIZE - offset_size);

    size_t size_left = size - (BLOCK_SIZE - offset_size);

    size_t n_blocks = (size_left / BLOCK_SIZE) + offset_blocks + 1;
    size_left = size_left % BLOCK_SIZE;

    for (size_t i = offset_blocks + 1; i < n_blocks; i++) {
        memcpy(buffer + offset_size + (i * BLOCK_SIZE),
               BLOCKS[inode->blocks[i]], BLOCK_SIZE);
    }
    memcpy(buffer + (n_blocks * BLOCK_SIZE), BLOCKS[inode->blocks[n_blocks]],
           size_left);

    inode->data_access = time(NULL);

    return size;
}

int fisopfs_write(const char* path, const char* buffer, size_t size,
                  off_t offset, struct fuse_file_info* fi)
{
    printf("[debug] fisopfs_write - path: %s, offset: %lu, size: %lu\n", path,
           offset, size);

    inode_t* inode = &INODES[fi->fh];

    size_t offset_blocks = offset / BLOCK_SIZE;
    if (offset_blocks > inode->size_blocks) {
        printf("SEGMENTATION FAULT\n");
        return -1;
    }

    size_t offset_size = offset % BLOCK_SIZE;
    if (BLOCK_SIZE > offset_size + size) {
        memcpy(BLOCKS[inode->blocks[offset_blocks]] + offset_size, buffer,
               size);
        inode->len += size;
        return size;
    }

    strncpy(BLOCKS[inode->blocks[offset_blocks]] + offset_size, buffer,
            BLOCK_SIZE - offset_size);
    size_t size_left = size - (BLOCK_SIZE - offset_size);

    size_t n_blocks = size_left / BLOCK_SIZE;
    size_left = size_left % BLOCK_SIZE;

    int block_index;
    if (size_left != 0) {
        block_index = allocate_elements(&block_bitmap, n_blocks + 1);
    }
    else {
        block_index = allocate_elements(&block_bitmap, n_blocks);
    }

    if (block_index < 0) return -ENOMEM;

    for (size_t i = 0; i < n_blocks; i++) {
        memcpy(BLOCKS[block_index + i], (i * BLOCK_SIZE) + buffer, BLOCK_SIZE);
        inode->blocks[inode->size_blocks + i] = block_index + i;
    }
    memcpy(BLOCKS[block_index + n_blocks], (n_blocks * BLOCK_SIZE) + buffer,
           size_left);
    inode->blocks[inode->size_blocks + n_blocks] = block_index + n_blocks;

    inode->size_blocks += n_blocks;
    if (size_left != 0) { inode->size_blocks += 1; }

    inode->len += size;
    inode->data_modification = time(NULL);
    return size;
}

int fisopfs_unlink(const char* path)
{
    printf("[debug] fisopfs_unlink - path: %s \n", path);

    char directory[MAX_PATH_LEN];
    char filename[MAX_NAME_LEN];
    parse_path(path, directory, filename);

    int inode_index = get_inode(directory);
    if (inode_index < 0) return -ENOENT;

    return remove_dir_entry(filename, inode_index);
}

int fisopfs_rmdir(const char* path)
{
    printf("[debug] fisopfs_rmdir - path: %s \n", path);

    char directory[MAX_PATH_LEN];
    char dirname[MAX_NAME_LEN];
    parse_path(path, directory, dirname);

    int inode_index = get_inode(directory);
    if (inode_index < 0) return -ENOENT;

    return remove_dir_entry(dirname, inode_index);
}

int fisopfs_open(const char* path, struct fuse_file_info* fi)
{
    printf("[debug] fisopfs_open - path: %s, flags: %d\n", path, fi->flags);

    int inode_index = get_inode(path);
    if (inode_index < 0) return -ENOENT;

    fi->fh = inode_index;

    return 0;
}

int fisopfs_create(const char* path, mode_t mode, struct fuse_file_info* fi)
{
    printf("[debug] fisopfs_create - path: %s, mode %d\n", path, mode);

    char filename[MAX_NAME_LEN];
    char directory[MAX_PATH_LEN];
    parse_path(path, directory, filename);

    int parent_index = get_inode(directory);
    if (parent_index < 0) return -ENOENT;

    int inode_index = allocate_elements(&inode_bitmap, 1);
    if (inode_index < 0) return -ENOMEM;

    int block_index = allocate_elements(&block_bitmap, 1);
    if (block_index < 0) return -ENOMEM;

    int res = add_dir_entry(filename, parent_index, inode_index);
    if (res < 0) return -ENOENT;

    inode_t* inode = &INODES[inode_index];
    inode->mode = mode;
    inode->size_blocks = 1;
    inode->blocks[0] = block_index;
    inode->len = 0;
    inode->n_links = 1;
    inode->data_change = time(NULL);
    inode->data_access = time(NULL);
    inode->data_modification = time(NULL);
    inode->uid = getuid();
    inode->gid = getgid();

    fi->fh = inode_index;

    return 0;
}

int fisopfs_link(const char* old_file, const char* new_file)
{
    printf("[debug] fisopfs_link - old_file: %s, new_file: %s \n", old_file,
           new_file);

    int old_inode_index = get_inode(old_file);
    if (old_inode_index < 0) return -ENOENT;

    char new_directory[MAX_PATH_LEN];
    char new_filename[MAX_NAME_LEN];
    parse_path(new_file, new_directory, new_filename);

    int new_dir_inode_index = get_inode(new_directory);
    if (new_dir_inode_index < 0) return -ENOENT;

    INODES[old_inode_index].n_links += 1;
    add_dir_entry(new_filename, new_dir_inode_index, old_inode_index);

    return 0;
}

int fisopfs_utimens(const char* path_archivo, const struct timespec tv[2])
{
    printf("[debug] fisopfs_utimens: %s\n", path_archivo);
    int inode_index = get_inode(path_archivo);
    INODES[inode_index].data_access = tv[0].tv_sec;
    INODES[inode_index].data_modification = tv[1].tv_sec;
    return 0;
}

int fisopfs_flush(const char* path_archivo, struct fuse_file_info* f_info)
{
    char cwd[1024];
    char filepath[2048];

    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        strcpy(filepath, cwd);
        strcat(filepath, "/fs.fisopfs");
    }

    int fd = open(filepath, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fd < 0) { return -1; }

    write(fd, INODES, MAX_INODES * sizeof(inode_t));
    write(fd, BLOCKS, MAX_BLOCKS * BLOCK_SIZE);
    write(fd, inode_bitmap.bits,
          ((inode_bitmap.size + 31) / 32) * sizeof(unsigned int));
    write(fd, block_bitmap.bits,
          ((block_bitmap.size + 31) / 32) * sizeof(unsigned int));

    close(fd);
    return 0;
}

struct fuse_operations operations = {
    .getattr = fisopfs_getattr,
    .readdir = fisopfs_readdir,
    .read = fisopfs_read,
    .write = fisopfs_write,
    .mkdir = fisopfs_mkdir,
    .open = fisopfs_open,
    .init = fisopfs_init,
    .destroy = fisopfs_destroy,
    .create = fisopfs_create,
    .utimens = fisopfs_utimens,
    .unlink = fisopfs_unlink,
    .link = fisopfs_link,
    .rmdir = fisopfs_rmdir,
    .flush = fisopfs_flush,
};

int main(int argc, char* argv[])
{
    return fuse_main(argc, argv, &operations, NULL);
}
