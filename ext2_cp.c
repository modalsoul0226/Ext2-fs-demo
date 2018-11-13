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
    if(argc != 4) {
        fprintf(stderr, "Usage: %s <image file name> <src from native OS> <dest to ext2>\n", argv[0]);
        exit(1);
    }

    char *src = argv[2];
    char *dest = argv[3];

    /* A binary flag indicating whether the last token is a dir or not. */
    int last_dir = 0;
    if (dest[0] != '/') {
        fprintf(stderr, "No such file or directory.\n");
        exit(ENOENT);
    }
    if (dest[strlen(dest) - 1] == '/') last_dir = 1;

    /* Initialize various base pointers. */
    int img_fd = open(argv[1], O_RDWR);
    ext2_base_init(img_fd);

    /* Open the src file. */
    int src_fd = open(src, O_RDONLY);
    if (src_fd < 0) {
        perror("open");
        exit(1);
    }

    /* Obtain the file size. */
    struct stat st;
    stat(src, &st);
    unsigned int src_size = (unsigned int)st.st_size;

    /* A linked list representation of the absolute path. */
    path_node_list path_list = { .head = NULL, .tail = NULL};
    split_path(dest, &path_list);

    if (path_list.tail && strlen(path_list.tail->name) > EXT2_NAME_LEN) {
        fprintf(stderr, "File name too long.\n");
        exit(1);
    }

    /* Get the inode number of the parent directory. */
    char *dest_name = NULL;
    int inode_num = 2;
    if (path_list.head != path_list.tail) {
        inode_num = get_inode_num(2, path_list.head, &path_list, 0);

        if (inode_num < 0) {
            fprintf(stderr, "No such file or directory.\n");
            exit(ENOENT);
        }
    }

    int temp_inode_num = 0;
    /* Check whether the specified file already exists. */
    if ((path_list.head) && (find_dir(inode_num, path_list.tail->name, 1) > 0)) {
        fprintf(stderr, "File or directory already exists.\n");
        exit(EEXIST);

    } else if ((!path_list.head) ||
            ((temp_inode_num = find_dir(inode_num, path_list.tail->name, 0)) > 0)) {
        /* Check whether there is a directory with the same name. */
        if (path_list.head) inode_num = temp_inode_num;

        path_node_list src_list = { .head = NULL, .tail = NULL};
        split_path(src, &src_list);
        assert(src_list.head != NULL);
        assert(src_list.tail != NULL);
        dest_name = src_list.tail->name;

    } else {
        /* If there does not exist a directory with the specified name */
        if (last_dir) {
            fprintf(stderr, "No such file or directory.\n");
            exit(ENOENT);
        }
        dest_name = path_list.tail->name;
    }

    if (find_dir(inode_num, dest_name, 2) > 0) {
        fprintf(stderr, "File or directory already exists.\n");
        exit(EEXIST);
    }

    /* Allocate space for file on the disk. */ 
    allocate_dir((unsigned int) inode_num, dest_name, 1, src_size, src_fd);

    /* Clean up */
    close(img_fd);
    close(src_fd);
    free_path_list(&path_list);
    munmap(disk, 128 * 1024);
}