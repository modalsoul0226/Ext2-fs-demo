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
    int src_inode = 0;
    int dest_parent = 2;
    char *src = NULL;
    char* dest = NULL;
    /* A binary flag indicating whether the last token is a dir or not. */
    int symb_enable = 0;

    /* Get the src and dest. */
    if (argc == 4) {
        src = argv[2];
        dest = argv[3];

    } else if (argc == 5) {
        if (strcmp(argv[2], "-s") != 0) {
            fprintf(stderr, "Please use flag <-s> if wish to create a symbolic link.\n");
            exit(1);
        }
        src = argv[3];
        dest = argv[4];
        symb_enable = 1;
    } else {
        fprintf(stderr, "Usage: %s <image file name> <-s> <src> <dest>\n", argv[0]);
        exit(1);
    }

    if (dest[0] != '/') {
        fprintf(stderr, "No such file or directory.\n");
        exit(ENOENT);
    }

    /* A binary flag indicating whether the last token is a dir or not. */
    int last_dir = 0;
    if (dest[strlen(dest) - 1] == '/') last_dir = 1;

    /* Initialize various base pointers. */
    int img_fd = open(argv[1], O_RDWR);
    ext2_base_init(img_fd);

    /* Linked list representations of the absolute path for src and dest respectively. */
    path_node_list src_path_list = { .head = NULL, .tail = NULL};
    split_path(src, &src_path_list);

    path_node_list dest_path_list = { .head = NULL, .tail = NULL};
    split_path(dest, &dest_path_list);

    if (dest_path_list.tail && strlen(dest_path_list.tail->name) > EXT2_NAME_LEN) {
        fprintf(stderr, "Link's name too long.\n");
        exit(1);
    }

    int src_not_dir = 0;
    /* Get the inode number of src file/dir. */
    if (src_path_list.head == NULL) src_inode = 2;
    else {
        int src_parent = get_inode_num(2, src_path_list.head, &src_path_list, 2);
        if (src_parent < 0) {
            fprintf(stderr, "No such file or directory.\n");
            exit(ENOENT);
        }
        if (!symb_enable && find_dir(src_parent, src_path_list.tail->name, 0) > 0) {
            fprintf(stderr, "Cannot create a hard link for a directory.\n");
            exit(EISDIR);
        }
        if (find_dir(src_parent, src_path_list.tail->name, 0) < 0) src_not_dir = 1;
        src_inode = find_dir(src_parent, src_path_list.tail->name, 2);
    }

    /* Get the inode number of dest's parent directory. */
    char *dest_name = NULL;
    int temp_inode_num = 0;
    if (dest_path_list.head != dest_path_list.tail) {
        dest_parent = get_inode_num(2, dest_path_list.head, &dest_path_list, 0);
        if (dest_parent < 0) {
            fprintf(stderr, "No such file or directory.\n");
            exit(ENOENT);
        }
    }
    /* Check whether the specified file already exists. */
    if ((dest_path_list.head) && (find_dir(dest_parent, dest_path_list.tail->name, 1) > 0)) {
        fprintf(stderr, "File or directory already exists.\n");
        exit(EEXIST);

    } else if ((!dest_path_list.head) ||
               ((temp_inode_num = find_dir(dest_parent, dest_path_list.tail->name, 0)) > 0)) {
        /* Check whether there is a directory with the same name. */
        if (dest_path_list.head) {
            dest_parent = temp_inode_num;
        }

        if (src_path_list.tail) dest_name = src_path_list.tail->name;
        else dest_name = "/";

    } else {
        /* If there does not exist a directory with the specified name */
        if (last_dir && !symb_enable) {
            fprintf(stderr, "No such file or directory.\n");
            exit(ENOENT);
        } else if (last_dir && src_not_dir) {
            fprintf(stderr, "No such file or directory.\n");
            exit(ENOENT);
        }
        dest_name = dest_path_list.tail->name;
    }

    if (find_dir(dest_parent, dest_name, 2) > 0) {
        fprintf(stderr, "File or directory already exists.\n");
        exit(EEXIST);
    }

    /* If the path checks are all OK, link the src to dest. */
    if (symb_enable) alloc_symblink(src, dest_parent, dest_name);
    else alloc_hardlink(src_inode, dest_parent, dest_name);

    /* Clean up */
    close(img_fd);
    free_path_list(&src_path_list);
    free_path_list(&dest_path_list);
    munmap(disk, 128 * 1024);
}