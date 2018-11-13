#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <assert.h>
#include <errno.h>
#include "ext2.h"


int main(int argc, char **argv) {
    if(argc != 3) {
        fprintf(stderr, "Usage: %s <image file name> <absolute path>\n", argv[0]);
        exit(1);
    }

    char *absolute_path = argv[2];
    if (absolute_path[0] != '/') {
        fprintf(stderr, "No such file or directory.\n");
        exit(ENOENT);
    }

    /* Initialize various base pointers. */
    int fd = open(argv[1], O_RDWR);
    ext2_base_init(fd);

    /* A linked list representation of the absolute path. */
    path_node_list path_list = { .head = NULL, .tail = NULL};
    split_path(absolute_path, &path_list);

    if (path_list.head == NULL) {
        fprintf(stderr, "File or directory already exists.\n");
        exit(EEXIST);
    }
    assert(path_list.head != NULL);
    assert(path_list.tail != NULL);


    /* Get the inode number of the parent directory. */
    int inode_num = 2;

    if (path_list.head != path_list.tail) {
        inode_num = get_inode_num(2, path_list.head, &path_list, 0);

        if (inode_num < 0) {
            fprintf(stderr, "No such file or directory.\n");
            exit(ENOENT);
        }
    }

    /* Check whether the specified dir already exists. */
    if (find_dir(inode_num, path_list.tail->name, 2) > 0) {
        fprintf(stderr, "File or directory already exists.\n");
        exit(EEXIST);
    }

    /* Allocate space for new directory from the disk. */
    allocate_dir((unsigned int)inode_num, path_list.tail->name, 0, EXT2_BLOCK_SIZE, 0);

    /* Clean up */
    close(fd);
    free_path_list(&path_list);
    munmap(disk, 128 * 1024);
}