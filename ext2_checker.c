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
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <image file name> \n", argv[0]);
        exit(1);
    }

    /* Initialize various base pointers. */
    int img_fd = open(argv[1], O_RDWR);
    ext2_base_init(img_fd);

    int fix_sum = 0;
    /* Fix the counters. */
    fix_sum += (check_counter(0) + check_counter(1));
    /* Fix other inconsistencies. */
    fix_sum += checker(2);

    if (fix_sum) {
        printf("%d file system inconsistencies repaired!\n", fix_sum);
    } else{
        printf("No file system inconsistencies detected!\n");
    }

    /* Clean up */
    close(img_fd);
    munmap(disk, 128 * 1024);
}