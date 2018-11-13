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
#include <time.h>
#include "ext2.h"

/*
 * A recursive routine to remove a directory and all the entities under this directory. 
 */
void dealloc_dir(unsigned int inode_num, unsigned int parent_inode_num, char *inode_name) {
    if (inode_num == 2 || inode_num == 11) {
        fprintf(stderr, "Sorry, cannot remove root directory or lost+found.\n");
        exit(1);
    }

    struct ext2_inode *target_inode = inode_table + inode_num - 1;
    struct ext2_inode *parent = inode_table + parent_inode_num - 1;

    /* Get the i_block's we need to iterate later. */
    int block_list[128];
    memset(block_list, 0, 128 * sizeof(int));
    clear_block_list();
    dfs(target_inode->i_block);
    int block_list_size = get_block_nums(block_list);

    if (block_list_size > 12) set_specific_bit(target_inode->i_block[12], 0, 1);
    for (int i = 0; i < block_list_size; i++) {
        int len_sum = 0;
        struct ext2_dir_entry *dir_entry = (struct ext2_dir_entry *)
                (disk + block_list[i] * EXT2_BLOCK_SIZE);

        if (i == 0) {
            /* Check the dir itself and skip the second entry. */
            len_sum += dir_entry->rec_len;
            dir_entry = (struct ext2_dir_entry *)((unsigned char *)dir_entry + dir_entry->rec_len);
            len_sum += dir_entry->rec_len;
            dir_entry = (struct ext2_dir_entry *)((unsigned char *)dir_entry + dir_entry->rec_len);
        }

        /* Check whether the dir_entry is valid or not. */
        if (!check_dir_entry(dir_entry, 1)) break;
        else if (!dir_entry->inode) {
            len_sum += dir_entry->rec_len;
            dir_entry = (struct ext2_dir_entry *)((unsigned char *)dir_entry + dir_entry->rec_len);
        }

        while (len_sum != EXT2_BLOCK_SIZE) {
            if (!check_dir_entry(dir_entry, 1)) break;

            /* Get the name of the dir_entry. */
            char name[dir_entry->name_len + 1];
            strncpy(name, dir_entry->name, dir_entry->name_len);
            name[dir_entry->name_len] = '\0';

            /* Remove the files and directories recursively. */
            if (dir_entry->file_type == EXT2_FT_DIR) {
                dealloc_dir(dir_entry->inode, inode_num, name);
            } else {
                dealloc_file(dir_entry->inode, inode_num, name);
            }
            len_sum += dir_entry->rec_len;
            dir_entry = (struct ext2_dir_entry *)
                    ((unsigned char *)dir_entry + dir_entry->rec_len);
        }
    }
    /* Update various counters. */
    assert(target_inode->i_links_count > 0);
    target_inode->i_links_count = 0;
    target_inode->i_dtime = (unsigned int)time(NULL);
    parent->i_links_count--;
    gd->bg_used_dirs_count--;

    /* Update the inode bitmap. */
    set_specific_bit(inode_num, 0, 0);
    /* Clear i_block bitmap. */
    memset(block_list, 0, 128 * sizeof(int));
    clear_block_list();
    dfs(target_inode->i_block);
    block_list_size = get_block_nums(block_list);

    /* Update the block btimap. */
    if (block_list_size > 12) set_specific_bit(target_inode->i_block[12], 0, 1);
    for (int i = 0; i < block_list_size; i++) {
        set_specific_bit(block_list[i], 0, 1);
    }

    /* Lastly, remove the folder itself from the parent directory's dir_entry block. */
    dealloc_dir_block(parent_inode_num, inode_name);
}


int main(int argc, char **argv) {
    if(argc != 4) {
        fprintf(stderr, "Usage: %s <image file name> <-r> <absolute path of a file>\n", argv[0]);
        exit(1);
    }

    char *flag = argv[2];
    char *target = argv[3];

    if (strlen(flag) != 2 || strncmp(flag, "-r", 2) != 0) {
        fprintf(stderr, "Usage: %s <image file name> <-r> <absolute path of a file>\n", argv[0]);
        exit(1);
    }

    if (target[0] != '/') {
        fprintf(stderr, "No such file or directory.\n");
        exit(ENOENT);
    }

    /* Initialize various base pointers. */
    int img_fd = open(argv[1], O_RDWR);
    ext2_base_init(img_fd);

    /* A linked list representation of the absolute path. */
    path_node_list path_list = { .head = NULL, .tail = NULL};
    split_path(target, &path_list);

    if (path_list.head == NULL) {
        fprintf(stderr, "Cannot remove the root directory.\n");
        exit(EISDIR);
    }

    /* Get the inode number of the parent directory. */
    int parent_inode = get_inode_num(2, path_list.head, &path_list, 2);
    if (parent_inode < 0) {
        fprintf(stderr, "No such file or directory.\n");
        exit(ENOENT);
    }

    /* Check if the file/dir we want to remove does exists. */
    int inode_num;
    if ((inode_num = find_dir(parent_inode, path_list.tail->name, 2)) < 0) {
        fprintf(stderr, "No such file or directory.\n");
        exit(ENOENT);
    }

    if (find_dir(parent_inode, path_list.tail->name, 0) > 0) {
        dealloc_dir((unsigned int)inode_num, (unsigned int)parent_inode, path_list.tail->name);
    } else {
        dealloc_file((unsigned int) inode_num, (unsigned int) parent_inode, path_list.tail->name);
    }

    close(img_fd);
}

