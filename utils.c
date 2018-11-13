/*
 * utils.c contains functions for different purposes of utilities.
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
#include <asm/errno.h>
#include "ext2.h"

extern unsigned int inode_num;
extern unsigned int parent_inode_num;
extern char *i_name;

void ext2_base_init(int fd) {
    /* Initialize the pointer to disk. */
    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
    /* Start of the super block. */
    sb = (struct ext2_super_block *)(disk + 1024);
    /* Start of the group desciptor block. */
    gd = (struct ext2_group_desc *)(sb + 1);
    /* The inode table. */
    inode_table = (struct ext2_inode *) (disk + gd->bg_inode_table * EXT2_BLOCK_SIZE);
    
    unsigned int block_bitmap_offset = gd->bg_block_bitmap;
    unsigned int inode_bitmap_offset = gd->bg_inode_bitmap;

    block_bitmap_base = disk + block_bitmap_offset * EXT2_BLOCK_SIZE;
    inode_bitmap_base = disk + inode_bitmap_offset * EXT2_BLOCK_SIZE;
}

/* The free routine in order to avoid memory leak. */
void free_path_list(path_node_list *path_list) {
    path_node *curr_node = path_list->head;

    while (curr_node) {
        path_node *temp = curr_node->next;
        free(curr_node);
        curr_node = temp;
    }
}

/* Check whether a directory entry is valid or not. */
int check_dir_entry(struct ext2_dir_entry* dir_entry, int block_enable) {
    if (dir_entry->inode < 0 || dir_entry->inode > 32) {
        /* A directory entry's inode number can be 0 if it is the
         * first entry of a data block other than the first one and is removed. */
        return 0;
    } else if (dir_entry->rec_len > EXT2_BLOCK_SIZE) {
        return 0;
    } else if (dir_entry->name_len > EXT2_NAME_LEN) {
        return 0;
    }

    return 1;
}

/*
 * Recover the entry with given name in the dir_entry.
 *
 * Return the inode_num on success, 0 on fail.
 */
int recover_entry(struct ext2_dir_entry *dir_entry, char *name, int length) {
    assert(name[strlen(name)] == '\0');
    int len_sum = 0;
    int base_len = sizeof(unsigned int) + sizeof(unsigned short) +
                   2 * sizeof(unsigned char);
    struct ext2_dir_entry *cur_entry = dir_entry;

    /* Loop over a dir_entry's rec_len. */
    while (len_sum < length) {
        if (!check_dir_entry(cur_entry, 0)) return 0;
        int real_len = base_len + cur_entry->name_len;
        while (real_len % 4) real_len++;

        if (strlen(name) != cur_entry->name_len ||
                strncmp(name, cur_entry->name, strlen(name)) != 0) {
            len_sum += real_len;
            cur_entry = (struct ext2_dir_entry *)
                    ((unsigned char *)cur_entry + real_len);
        } else {
            dir_entry->rec_len = (unsigned short)len_sum;
            cur_entry->rec_len = (unsigned short)(length - len_sum);
            return cur_entry->inode;
        }
    }
    return 0;
}

/*
 * Restore a specified file.
 *
 * Return the inode number on success.
 * Return 0 on fail.
 */
int restore(int p_inode_num, char *target_name) {
    struct ext2_inode *parent_inode = inode_table + p_inode_num - 1;
    struct ext2_dir_entry *dir_entry = NULL;
    int target_inode_num = 0;

    /* Get the i_block numbers. */
    int block_list[128];
    memset(block_list, 0, 128 * sizeof(int));
    clear_block_list();
    dfs(parent_inode->i_block);
    int block_list_size = get_block_nums(block_list);

    /* Loop over the entry and try to recover the target. */
    for (int i = 0; i < block_list_size; i++) {
        int len_sum = 0;
        dir_entry = (struct ext2_dir_entry *)
                (disk + block_list[i] * EXT2_BLOCK_SIZE);

        while (len_sum != EXT2_BLOCK_SIZE) {
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
    if (target_inode->i_mode & EXT2_S_IFDIR) {
        fprintf(stderr, "Cannot restore a directory.\n");
        exit(EISDIR);
    }

    /* Check whether the file is recoverable or not. */
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

    return target_inode_num;
}


/*
 * Check whether the superblock and block group counters for
 * free blocks and free inodes match the number of free inodes and
 * data blocks indicated in the respective bitmaps.
 */
int check_counter(int block_enable) {
    int fix_sum = 0;
    int loops = 0;
    int bit_free_count = 0;

    unsigned int *sb_free_count = NULL;
    unsigned short *gd_free_count = NULL;

    int value_idx = 0;
    char *x_value[2] = {"Super block ", "Block group"};
    char *y_value[2] = {"free inodes", "free blocks"};

    if (block_enable) {
        loops = sb->s_blocks_count;
        sb_free_count = &(sb->s_free_blocks_count);
        gd_free_count = &(gd->bg_free_blocks_count);
    } else {
        loops = sb->s_inodes_count;
        sb_free_count = &(sb->s_free_inodes_count);
        gd_free_count = &(gd->bg_free_inodes_count);
    }

    for (int i = 1; i <= loops; i++) {
        if (!check_bit(i, block_enable)) bit_free_count++;
    }

    int sb_off = bit_free_count - *sb_free_count;
    if (sb_off) {
        *sb_free_count = (unsigned int)bit_free_count;
        fix_sum += abs(sb_off);
        printf("Fixed: %s's %s counter was off by %d compared to the bitmap\n",
               x_value[value_idx], y_value[block_enable], abs(sb_off));
    }

    int gd_off = bit_free_count - *gd_free_count;
    if (gd_off) {
        value_idx = 1;
        *gd_free_count = (unsigned short)bit_free_count;
        fix_sum += abs(gd_off);
        printf("Fixed: %s's %s counter was off by %d compared to the bitmap\n",
               x_value[value_idx], y_value[block_enable], abs(sb_off));
    }
    return fix_sum;
}

/*
 * Check the filesystem using DFS.
 *
 * Check the dir entries of the specified inode number.
 */
int checker(int i_node_num) {
    int fix_sum = 0;

    struct ext2_inode *inode = inode_table + i_node_num - 1;
    assert(inode->i_mode & EXT2_S_IFDIR);

    int block_list[128];
    memset(block_list, 0, 128 * sizeof(int));
    clear_block_list();
    dfs(inode->i_block);
    int block_list_size = get_block_nums(block_list);

    /* Iterate all block_num's in the i_block. */
    for (int i = 0; i < block_list_size; i++) {
        int len_sum = 0;
        struct ext2_dir_entry *dir_entry = (struct ext2_dir_entry *)
                (disk + block_list[i] * EXT2_BLOCK_SIZE);

        /* If this is the first block_num in the i_block. */
        if (i == 0) {
            /* Check the dir itself and skip the second entry. */
            len_sum += dir_entry->rec_len;
            fix_sum += do_the_checkings(dir_entry);

            dir_entry = (struct ext2_dir_entry *)((unsigned char *)dir_entry + dir_entry->rec_len);
            len_sum += dir_entry->rec_len;
            dir_entry = (struct ext2_dir_entry *)((unsigned char *)dir_entry + dir_entry->rec_len);
        }

        /* Iterate over all dir entries stored in this block_num. */
        while (len_sum != EXT2_BLOCK_SIZE) {
            /* Do the checkings. */
            fix_sum += do_the_checkings(dir_entry);

            int i_num = dir_entry->inode;
            if (inode_table[i_num - 1].i_mode & EXT2_S_IFDIR) {
                if (i_num != 11) fix_sum += checker(i_num);
            }

            len_sum += dir_entry->rec_len;
            dir_entry = (struct ext2_dir_entry *)
                    ((unsigned char *)dir_entry + dir_entry->rec_len);
        }
    }

    return fix_sum;
}

int do_the_checkings(struct ext2_dir_entry *dir_entry) {
    int fix_sum = 0;

    fix_sum += check_bitmap(dir_entry, 0);
    fix_sum += check_bitmap(dir_entry, 1);
    fix_sum += check_mode(dir_entry);
    fix_sum += check_dtime(dir_entry);

    return fix_sum;
}

/* Check whether the i_mode match file_type. If not, fix it and report the fix sums. */
int check_mode(struct ext2_dir_entry *dir_entry) {
    int fix_sum = 0;
    /* Skip if the first entry is removed before. */
    if (!dir_entry->inode) return 0;
    struct ext2_inode *inode = inode_table + dir_entry->inode - 1;

    if (inode->i_mode & EXT2_S_IFREG) {
        if (dir_entry->file_type != EXT2_FT_REG_FILE) {
            fix_sum++;
            dir_entry->file_type = EXT2_FT_REG_FILE;
        }

    } else if (inode->i_mode & EXT2_S_IFLNK) {
        if (dir_entry->file_type != EXT2_FT_SYMLINK) {
            fix_sum++;
            dir_entry->file_type = EXT2_FT_SYMLINK;
        }

    } else if (inode->i_mode & EXT2_S_IFDIR) {
        if (dir_entry->file_type != EXT2_FT_DIR) {
            fix_sum++;
            dir_entry->file_type = EXT2_FT_DIR;
        }
    }

    if (fix_sum) printf("Fixed: Entry type vs inode mismatch: inode [%d]\n", dir_entry->inode);
    return fix_sum;
}


/* Check whether the bitmap is correct according to each dir_entry. */
int check_bitmap(struct ext2_dir_entry *dir_entry, int block_enable) {
    int fix_sum = 0;
    int i_num = dir_entry->inode;
    /* Skip if the first entry is removed before. */
    if (!i_num) return 0;
    struct ext2_inode *inode = inode_table + i_num - 1;

    if (block_enable) {

        int block_list[128];
        memset(block_list, 0, 128 * sizeof(int));
        clear_block_list();
        dfs(inode->i_block);
        int block_list_size = get_block_nums(block_list);

        for (int i = 0; i < block_list_size; i++) {
            if (!check_bit(block_list[i], block_enable)) {
                set_specific_bit(block_list[i], 1, block_enable);
                fix_sum++;
            }
        }
        if (fix_sum) printf("Fixed: %d in-use data blocks not marked in data bitmap for inode: [%d]\n", fix_sum, i_num);
    } else {
        if (!check_bit(i_num, 0)) {
            set_specific_bit(i_num, 1, block_enable);
            fix_sum++;
        }
        if (fix_sum) printf("Fixed: valid inode marked for deletion: [%d]\n", i_num);
    }
    return fix_sum;
}

/* Check whether the dtime is correctly set or not. */
int check_dtime(struct ext2_dir_entry *dir_entry) {
    int fix_sum = 0;
    if (!dir_entry->inode) return 0;
    struct ext2_inode *inode = inode_table + dir_entry->inode - 1;

    if (inode->i_dtime) {
        inode->i_dtime = 0;
        fix_sum++;
        printf("Fixed: valid inode marked for deletion: [%d]\n", dir_entry->inode);
    }
    return fix_sum;
}
