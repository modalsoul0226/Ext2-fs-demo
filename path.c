/*
 * path.c contains helper functions for path
 * analysis and manipulations.
 *
 * Created by Xinze Zhao and Zimu Liu.
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <assert.h>
#include "ext2.h"


int block_list[128];
int block_list_size = 0;
int parent_i_block = 0;
/* Index of the first 0 entry in parent's i_block. */
int last_idx = 0;

/*
 * Clear the global variables.
 */
void clear_block_list() {
    /* Clear whatever that is stored in block_list. */
    memset(block_list, 0, 128 * sizeof(int));
    block_list_size = 0;
}

/* Copy all the i_block numbers. */
int get_block_nums(int *block_nums) {
    for (int i = 0; i < block_list_size; i++) {
        block_nums[i] = block_list[i];
    }
    return block_list_size;
}

/* 
 * Split the absolute path and store the paths 
 * into path_node_list. 
 */
void split_path(char *absolue_path, path_node_list *path_list) {
    /* Sanity check. */
    assert(path_list->head == NULL);
    assert(path_list->tail == NULL);

    /* The index of writing. */
    int write_idx = 0;
    /* A binary flag indicating whether the head of path_list is set. */
    int head_set = 0;
    for (int i = 0; i < strlen(absolue_path); i++) {
        /* Check whether write_idx is legal. */
        assert(write_idx < MAX_NAME);

        /* Skip on slashes. */
        if (absolue_path[i] == '/') continue;

        /* If write_idx is 0, that means we need to initialize a
         * new node. */
        if (!write_idx) {
            path_node *node = malloc(sizeof(path_node));
            node->name[0] = '\0';
            node->next = NULL;

            if (!head_set) {
                /* If the head of path_list is not set yet. */
                path_list->head = node;
                head_set = 1;
            } else {
                path_list->tail->next = node;
            }
            /* Update the tail. */
            path_list->tail = node;
        }

        path_list->tail->name[write_idx] = absolue_path[i];

        if ((i + 1 == strlen(absolue_path)) || (absolue_path[i + 1] == '/')) {
            /* If write_idx is the last index of the name. */
            path_list->tail->name[write_idx + 1] = '\0';
            write_idx = 0;
        } else write_idx++;
    }
}


/*
 * Recursive routine to add the block numbers 
 * using depth-first-search.
 * Return 1 if it finishes printing.
 */
int dfs_visit(int block_num, int level) {
    /* The base case. */
    if (level == 0) {
        if (!block_num) return 1;
        else {
            block_list[block_list_size] = block_num;
            block_list_size++;
        }
    } else {
        /* Get the indirect block. */
        unsigned int *block_arr = (unsigned int *)(disk + EXT2_BLOCK_SIZE * block_num);
        int loops = EXT2_BLOCK_SIZE / sizeof(unsigned int);

        for (int i = 0; i < loops; i++) {
            if (dfs_visit(block_arr[i], level - 1)) {
                if (level == 1) {
                    /* Record the last indirect block. */
                    parent_i_block = block_num;
                    last_idx = i;
                }
                return 1;
            }
        }
    }

    return 0; 
}


/*
 * Add the block numbers of i_block to block_list.
 */
void dfs(unsigned int *i_block) {
    for (int i = 0; i < 15; i++) {
        if (i <= 11) {
            // print out the data blocks
            if (dfs_visit(i_block[i], 0)) {
                parent_i_block = 0;
                last_idx = i;
                break;
            }
        } else if (i == 12) {
            // print out the indirect blocks
            if (dfs_visit(i_block[i], 1)) break;
        } else if (i == 13) {
            // print out the double indirect blocks
            if (dfs_visit(i_block[i], 2)) break;
        } else {
            // print out the triple indirect blocks
            if (dfs_visit(i_block[i], 3)) break;
        }
    }
}


/* 
 * Subroutine of find_dir(), only search in one block for 
 * the directory with a given name. 
 * 
 * The flag file_enable is set as 1 when we want to also search whether
 * a file exists under a directory, set as 2 when we want to check whether a
 * file or a directory with specified name under the directory.
 *
 * Return the inode number
 * of the matched dir on success, -1 on fail.
 */
int find_dir_single_block(struct ext2_dir_entry *dir_entry, char *name, int file_enable) {
    int length = 0;

    while (length < EXT2_BLOCK_SIZE) {
        /* The flag is set when we find out the matched dir. */
        int flag = 1;
        /* Parameter name is null terminated. */
        int name_len = (int)strlen(name);
        if (name_len > EXT2_NAME_LEN) {
            fprintf(stderr, "Name too long.\n");
            exit(1);
        }

        /* If this entry is not a directory. */
        if (!file_enable && (dir_entry->file_type != EXT2_FT_DIR)) flag = 0;
        if (file_enable == 1 && (dir_entry->file_type != EXT2_FT_REG_FILE)) flag = 0;
        /* If length of the name is different. */ 
        if (name_len != dir_entry->name_len) flag = 0;

        if (flag) {
            char *dir_name = dir_entry->name;
            for (int i = 0; i < name_len; i++) {
                if (dir_name[i] != name[i]) flag = 0;
            }
        }

        if (flag) return dir_entry->inode;
        else {
            int rec_len = dir_entry->rec_len;
            length += rec_len;
            dir_entry = (struct ext2_dir_entry*)((unsigned char *)dir_entry + rec_len);
        }
    }
    /* If we get here, that indicates that 
     * we didn't find any matched dir. */
    return -1;
}


/* 
 * Check whether the specified name exists under the directory with 
 * the given inode_num. Return the inode number of the matched dir on
 * success, -1 on fail.
 */
int find_dir(int inode_num, char * name, int file_enable) {
    clear_block_list();
    dfs(inode_table[inode_num - 1].i_block);
    int loops = block_list_size;
    block_list_size = 0;
    
    for (int i = 0; i < loops; i++) {
        struct ext2_dir_entry *dir_entry = (struct ext2_dir_entry *)(disk + block_list[i] * EXT2_BLOCK_SIZE);
        int res = find_dir_single_block(dir_entry, name, file_enable);

        if (res > 0) return res; 
    }
    clear_block_list();
    return -1;
}


/* 
 * Get the inode_number of the parent dir of specified directory. 
 * 
 * Flag file_enable is set when the last entry of the path is a regular file.
 */
int get_inode_num(int inode_num, path_node *path, path_node_list *path_list, int file_enable) {
    if (path_list->head == path_list->tail) {
        if (find_dir(inode_num, path->name, file_enable) > 0) {
            return inode_num;
        } else return -1;
    } else if (path->next == path_list->tail) {
        return find_dir(inode_num, path->name, file_enable);
    } else {
        int res = find_dir(inode_num, path->name, file_enable);

        if (res < 0) return -1;
        else return get_inode_num(res, path->next, path_list, file_enable);
    }
}