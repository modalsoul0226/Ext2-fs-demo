#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include "ext2.h"

int main(int argc, char **argv) {
    if(argc != 3) {
        fprintf(stderr, "Usage: %s <image file name> <absolute path of a file>\n", argv[0]);
        exit(1);
    }

    char *target = argv[2];

    if (target[0] != '/') {
        fprintf(stderr, "No such file or directory.\n");
        exit(ENOENT);
    } else if (target[strlen(target) - 1] == '/'){
        fprintf(stderr, "Cannot remove a directory.\n");
        exit(EISDIR);
    }

    /* Initialize various base pointers. */
    int img_fd = open(argv[1], O_RDWR);
    ext2_base_init(img_fd);

    /* A linked list representation of the absolute path. */
    path_node_list path_list = { .head = NULL, .tail = NULL};
    split_path(target, &path_list);

    if (path_list.head == NULL) {
        fprintf(stderr, "Cannot remove a directory.\n");
        exit(EISDIR);
    }

    /* Get the inode number of the parent directory. */
    int parent_inode = get_inode_num(2, path_list.head, &path_list, 2);
    if (parent_inode < 0) {
        fprintf(stderr, "No such file or directory.\n");
        exit(ENOENT);
    }

    /* Check whether the target we want to remove is a directory or not. */
    if (find_dir(parent_inode, path_list.tail->name, 0) > 0) {
        fprintf(stderr, "Cannot remove a directory.\n");
        exit(EISDIR);
    }

    int inode_num;
    /* Check whether the file we want to remove does exist. */
    if ((inode_num = find_dir(parent_inode, path_list.tail->name, 2)) < 0) {
        fprintf(stderr, "No such file or directory.\n");
        exit(ENOENT);
    }

    /* Remove the specified file. */
    dealloc_file((unsigned int)inode_num, (unsigned int)parent_inode, path_list.tail->name);

    /* Clean up */
    close(img_fd);
    free_path_list(&path_list);
    munmap(disk, 128 * 1024);
}

