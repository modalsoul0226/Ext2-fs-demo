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

/* Restore the file with target_name under the directory with p_inode_num .*/
int restore_file(int p_inode_num, char *target_name) {
    struct ext2_inode *parent_inode = inode_table + p_inode_num - 1;
    struct ext2_dir_entry *dir_entry = NULL;
    int target_inode_num = 0;

    /* Get all the i_block numbers the parent has. */
    int block_list[128];
    memset(block_list, 0, 128 * sizeof(int));
    clear_block_list();
    dfs(parent_inode->i_block);
    int block_list_size = get_block_nums(block_list);

    /* Loop over the dir_entry's in order to search for the desired file/dir. */
    for (int i = 0; i < block_list_size; i++) {
        int len_sum = 0;
        dir_entry = (struct ext2_dir_entry *)
                (disk + block_list[i] * EXT2_BLOCK_SIZE);

        while (len_sum != EXT2_BLOCK_SIZE) {
            /* Recover all the available dir entries. */
            target_inode_num = recover_entry(dir_entry, target_name, dir_entry->rec_len);
            if (target_inode_num) break;

            len_sum += dir_entry->rec_len;
            dir_entry = (struct ext2_dir_entry *)
                    ((unsigned char *) dir_entry + dir_entry->rec_len);
        }
        if (target_inode_num) break;
    }


    /* We have found the inode number for the file we need to restore
     * if we proceed to this point. */
    if (!target_inode_num) return 0;
    if (check_bit(target_inode_num, 0)) return 0;

    struct ext2_inode *target_inode = inode_table + target_inode_num - 1;

    /* Set the bits. */
    memset(block_list, 0, 128 * sizeof(int));
    clear_block_list();
    dfs(target_inode->i_block);
    block_list_size = get_block_nums(block_list);
    for (int i = 0; i < block_list_size; i++) {
        if (check_bit(block_list[i], 1)) return 0;
    }
    /* The file is recoverable if we proceed to this point. */
    /* Update bitmap. */
    set_specific_bit(target_inode_num, 1, 0);
    if (block_list_size > 12) set_specific_bit(target_inode->i_block[12], 1, 1);
    for (int i = 0; i < block_list_size; i++) set_specific_bit(block_list[i], 1, 1);
    /* Update related fields. */
    target_inode->i_dtime = 0;
    target_inode->i_links_count = 1;
    if (target_inode->i_mode & EXT2_S_IFDIR) {
        target_inode->i_links_count = 2;
        parent_inode->i_links_count++;
        gd->bg_used_dirs_count++;
    }

    return target_inode_num;
}


/* 
 * Restore the directory recursively using the patent inode number and the target name. 
 */
int restore_dir(int parent_inode, char *target_name) {
    /* Initialize all the local variable. */
    int num_restored = 0; /* Number of entries that have been restored. */
    int inode_num = 0;
    if (!(inode_num = restore_file(parent_inode, target_name))) return 0;
    else num_restored++;

    int base_len = sizeof(unsigned int) + sizeof(unsigned short) +
                   2 * sizeof(unsigned char);

    struct ext2_inode *target_inode = inode_table + inode_num - 1;
    if (target_inode->i_mode & EXT2_S_IFREG) {
        return inode_num;
    }

    /* Get the i_block members for target inode. */
    int block_list[128];
    memset(block_list, 0, 128 * sizeof(int));
    clear_block_list();
    dfs(target_inode->i_block);
    int block_list_size = get_block_nums(block_list);

    if (block_list_size > 12) set_specific_bit(target_inode->i_block[12], 0, 1);

    /* Recursively restore all the files and directories under the current directory. */
    for (int i = 0; i < block_list_size; i++) {

        int len_sum = 0;
        struct ext2_dir_entry *dir_entry = (struct ext2_dir_entry *)
                (disk + block_list[i] * EXT2_BLOCK_SIZE);

        if (i == 0) {
            /* Check the dir itself and skip the second entry. */
            len_sum += 24;
            dir_entry = (struct ext2_dir_entry *)((unsigned char *)dir_entry + len_sum);
        }


        while (len_sum != EXT2_BLOCK_SIZE) {
            if (!check_dir_entry(dir_entry, 1)) {
                break;
            }

            /* Get the name of the dir_entry. */
            char name[dir_entry->name_len + 1];
            strncpy(name, dir_entry->name, dir_entry->name_len);
            name[dir_entry->name_len] = '\0';

            int real_len = base_len + dir_entry->name_len;
            while (real_len % 4) real_len++;

            /* the recursion part */
            if (dir_entry->file_type == EXT2_FT_DIR) {
                num_restored += restore_dir(inode_num, name);
            } else if (dir_entry->file_type == EXT2_FT_REG_FILE ||
                       dir_entry->file_type == EXT2_FT_SYMLINK) {
                num_restored += restore_file(inode_num, name);
            }

            dir_entry = (struct ext2_dir_entry *)
                    ((unsigned char *)dir_entry + real_len);
            len_sum += real_len;
        }
    }
    return num_restored;
}



int main(int argc, char **argv) {
    if(argc != 4) {
        fprintf(stderr, "Usage: %s <image file name> <-r> <absolute path of a file>\n", argv[0]);
        exit(1);
    }

    char *target = argv[3];

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
        fprintf(stderr, "Sorry, cannot restore the root directory.\n");
        return ENOENT;
    }

    /* Get the inode number of the parent directory. */
    int parent_inode = 2;
    if(path_list.head != path_list.tail) {
        parent_inode = get_inode_num(2, path_list.head, &path_list, 2);

        if (parent_inode < 0) {
            fprintf(stderr, "No such file or directory.\n");
            exit(ENOENT);
        }
    }

    /* Check whether there is a file or directory with the same name. */
    if (find_dir(parent_inode, path_list.tail->name, 2) > 0) {
        fprintf(stderr, "File or directroy already exists.\n");
        exit(EEXIST);
    }


    if (!restore_dir(parent_inode, path_list.tail->name)) {
        fprintf(stderr, "Sorry, the file is not recoverable.\n");
        exit(ENOENT);
    }

    /* Clean up */
    close(img_fd);
    free_path_list(&path_list);
    munmap(disk, 128 * 1024);
}



