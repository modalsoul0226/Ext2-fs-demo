/*
 * alloc.c contains functions for allocation/deallocation of files and
 * directories.
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
#include <time.h>
#include <asm/errno.h>
#include "ext2.h"

/* Basic information of a file/directory. */
unsigned int inode_num = 0;
unsigned int parent_inode_num = 0;
char *i_name = NULL;
unsigned int i_size = 0;
/* The array of block numbers that needs to be allocated. */
int block_num[32 - 11];
int block_num_size = 0;
int indirect_block_num = 0;

/* Block number of the block containing the last block of parent dir. */
extern int parent_i_block;
/* Index of the first 0 entry in parent's i_block. */
extern int last_idx;

/* 
 * ============= Main Driver function for this file ==============
 *  
 * Add a new directory to the disk under the path of a parent directory with
 * p_inode_num.
 * 
 * Do the same operations for a file instead of a dir if 
 * file_enable is set.
 */
void allocate_dir(unsigned int p_inode_num, char *name, int file_enable, unsigned int file_size, int src_fd) {
    /* Init some of the global variables. */
    int block_num = 0;
    int inode_num = 1;
    int size_left = file_size;
    while(size_left > 0) {
        size_left -= EXT2_BLOCK_SIZE;
        block_num++;
    }

    /* Check whether there is enough free space for the entity we want to add. */
    if (inode_num > sb->s_free_inodes_count || block_num > sb->s_free_blocks_count) {
        fprintf(stderr, "Disk quota exceeds!\n");
        exit(ENOSPC);
    }

    parent_inode_num = p_inode_num;
    i_name = name;
    i_size = file_size;

    if (!file_enable) {
        assert(src_fd == 0);
        gd->bg_used_dirs_count++;
    } else{
        assert(src_fd > 0);
    }

    /* Allocate an inode for it. */
    alloc_inode(file_enable);
    /* Allocate data block(s) for it. */
    alloc_block(file_enable, src_fd, NULL);
}

/*
 * Delete a file on the disk.
 */
void dealloc_file(unsigned int target_inode_num, unsigned int parent_inode, char *target_name) {
    inode_num = target_inode_num;
    parent_inode_num = parent_inode;
    i_name = target_name;

    struct ext2_inode *target_inode = inode_table + inode_num - 1;
    assert(target_inode->i_links_count > 0);
    target_inode->i_links_count--;
    if (!target_inode->i_links_count) {
        /* Set delete time. */
        target_inode->i_dtime = (unsigned int)time(NULL);
        /* Clear inode bitmap. */
        set_specific_bit(target_inode_num, 0, 0);
        
        /* Clear i_block bitmap. */
        int block_list[128];
        memset(block_list, 0, 128 * sizeof(int));
        clear_block_list();
        dfs(target_inode->i_block);
        int block_list_size = get_block_nums(block_list);
        if (block_list_size > 12) set_specific_bit(target_inode->i_block[12], 0, 1);
        for (int i = 0; i < block_list_size; i++) {
            set_specific_bit(block_list[i], 0, 1);
        }
        clear_block_list();
    }

    /* Update parent directory's data block in order to accomadate for the newly added entity. */
    dealloc_dir_block(parent_inode_num, i_name);
}

/*
 * Create a hard link for file with inode
 * number src_inode under the destination directory.
 */
void alloc_hardlink(int src_inode_num, int dest_parent, char *dest_name) {
    parent_inode_num = (unsigned int)dest_parent;
    inode_num = (unsigned int)src_inode_num;
    i_name = dest_name;
    update_parent_dir_block(1);

    struct ext2_inode *src_inode = inode_table + src_inode_num - 1;
    src_inode->i_links_count++;
}

/*
 * Create a symbolic link for file with inode number src_inode_num
 * under the destination directory.
 */
void alloc_symblink(char *src_path, int dest_parent, char *dest_name) {
    parent_inode_num = (unsigned int)dest_parent;
    i_name = dest_name;
    i_size = (unsigned int)strlen(src_path);

    alloc_inode(2);                 /* Allocate a new inode. */
    alloc_block(2, 0, src_path);    /* Allocate necessary data block(s). */
}


/*
 * Allocate block(s) for a directory or a regular file according to its size.
 */
void alloc_block(int file_enable, int src_fd, char *src_path) {
    int size_left = i_size;
    /* Binary flag indicating whether an indirect block is allocated. */
    int indirect_allocated = 0;
    int idx = 0;

    while (size_left > 0) {
        if (idx == 12 && !indirect_allocated) {
            /* Allocate an indirect block. */
            indirect_block_num = set_bitmap(1);
            indirect_allocated = 1;
        } else {
            block_num[idx] = set_bitmap(1);
            idx++;
            size_left -= EXT2_BLOCK_SIZE;
        }
    }
    if (indirect_allocated) assert(indirect_allocated != 0);

    block_num_size = idx;

    /* Initialize all the data blocks. */
    for (int i = 0; i < block_num_size; i++) {
        memset(disk + block_num[i] * EXT2_BLOCK_SIZE, 0, EXT2_BLOCK_SIZE);
    }

    /* Update the corresponding counts and bits. */
    update_inode_block();
    update_parent_inode(file_enable);
    if (file_enable == 0) {
        assert(src_fd == 0);
        init_dir_block();
    } else if (file_enable == 1) {
        assert(src_fd > 0);
        init_file_block(src_fd);
    } else {
        assert(src_path != NULL);
        init_symb_block(src_path);
    }
    /* Update the parent. */
    update_parent_dir_block(file_enable);
}

/*
 * Add a new inode to the inode table.
 */
void alloc_inode(int file_enable) {
    /* Initialize a new node. */
    struct ext2_inode new_inode;
    memset(&new_inode, 0, sizeof(struct ext2_inode));

    if (file_enable == 1) {
        /* A regular file. */
        new_inode.i_mode = EXT2_S_IFREG;
        new_inode.i_links_count = 1;
    } else if (file_enable == 0) {
        /* A directory. */
        new_inode.i_mode = EXT2_S_IFDIR;
        /* Default links count for every dir is 2. */
        new_inode.i_links_count = 2;
    } else {
        new_inode.i_mode = EXT2_S_IFLNK;
        new_inode.i_links_count = 1;
    }

    new_inode.i_size = i_size;

    inode_num = (unsigned int)set_bitmap(0);
    inode_table[inode_num - 1] = new_inode;
}


/*
 * Find the first one available from the corresponding bitmap.
 *
 * Flag block_enable is set when we wish to allocate from the block bitmap.
 */
int set_bitmap(int block_enable) {
    /* Check whether there are still free spaces. */
    if (block_enable && !sb->s_free_blocks_count) {
        fprintf(stderr, "No free blocks available.\n");
        exit(ENOSPC);

    } else if (!block_enable && !sb->s_free_inodes_count) {
        fprintf(stderr, "No free inodes available.\n");
        exit(ENOSPC);
    }

    int loops = 0;
    int inode_num = 0;
    unsigned char *bitmap_base = NULL;

    /* Differetiate between block bitmap and inode bitmap. */
    if (block_enable) {
        loops = sb->s_blocks_count / BYTE_IN_BITS;
        bitmap_base = block_bitmap_base;
        sb->s_free_blocks_count--;
        gd->bg_free_blocks_count--;
    } else {
        loops = sb->s_inodes_count / BYTE_IN_BITS;
        bitmap_base = inode_bitmap_base;
        sb->s_free_inodes_count--;
        gd->bg_free_inodes_count--;
    }

    /* Update the bitmap. */
    int bit_arr[8];
    for (int i = 0; i < loops; i++) {
        int dec = (int) *(bitmap_base + i);
        if (dec != 255) {
            convert_to_bin(bit_arr, dec);
        } else {
            inode_num += BYTE_IN_BITS;
            continue;
        }
        inode_num += set_bit(bit_arr);
        *(bitmap_base + i) = (unsigned char)convert_to_dec(bit_arr);

        break;
    }
    return inode_num;
}

/*
 * Convert the decimal number to binary and store those
 * figures in a bit array.
 */
void convert_to_bin(int *bit_arr, int dec) {
    for (int i = 0; i < BYTE_IN_BITS; i++) {
        bit_arr[i] = dec % 2;
        dec >>= 1;
    }
}

/*
 * Convert the binary figures to decimal representation.
 */
int convert_to_dec(int *bit_arr) {
    int res = 0;
    for (int i = 0; i < BYTE_IN_BITS; i++) {
        res += bit_arr[i] * (1 << i);
    }
    return res;
}

/*
 * Set the first available bit, and return the offset.
 */
int set_bit(int *bit_arr) {
    int res = 0;
    for (int i = 0; i < BYTE_IN_BITS; i++) {
        if (!bit_arr[i]) {
            bit_arr[i] = 1;
            res = i + 1;
            break;
        }
    }
    return res;
}

/*
 * Clear the specified bit in the bitmap.
 */
void set_specific_bit(int num, int bit, int block_enable) {
    unsigned char *bitmap_base = NULL;
    int inc;
    if(bit) inc = -1;
    else inc = 1;

    if (block_enable) {
        bitmap_base = block_bitmap_base;
        sb->s_free_blocks_count += inc;
        gd->bg_free_blocks_count += inc;
    } else {
        bitmap_base = inode_bitmap_base;
        sb->s_free_inodes_count += inc;
        gd->bg_free_inodes_count += inc;
    }

    int offset = num / 8;
    if (!(num % 8)) offset--;
    unsigned char *bits_base = bitmap_base + offset;
    int dec = *bits_base;

    int bit_arr[8];
    convert_to_bin(bit_arr, dec);

    int clear_index = num % 8 - 1;
    if (clear_index < 0) clear_index = 7;
    bit_arr[clear_index] = bit;
    *bits_base = (unsigned char)convert_to_dec(bit_arr);
}

/*
 * Check whether the specified bit is set or not.
 *
 * Return 1 if it is set, 0 if it is not.
 */
int check_bit(int num, int block_enable) {
    unsigned char *bitmap_base = NULL;

    if (block_enable) {
        bitmap_base = block_bitmap_base;
    } else {
        bitmap_base = inode_bitmap_base;
    }

    int offset = num / 8;
    if (!(num % 8)) offset--;

    unsigned char *bits_base = bitmap_base + offset;
    int dec = *bits_base;
    int bit_arr[8];
    convert_to_bin(bit_arr, dec);

    int index = num % 8 - 1;
    if (index < 0) index = 7;
    return bit_arr[index];
}

/*
 * Update the inode after allocating block(s) for a dir/file.
 */
void update_inode_block() {
    struct ext2_inode *inode = inode_table + inode_num - 1;
    inode->i_blocks = (unsigned int)block_num_size * 2;
    if (indirect_block_num) inode->i_blocks += 2;

    append_to_block(inode_num, block_num, block_num_size);
}

/* 
 * Initialize the dir block. We only need to initialize
 * one block for a directory.
 */
void init_dir_block() {
    /* Sanity check. */
    assert(block_num_size == 1);

    struct ext2_dir_entry *dir_entry = (struct ext2_dir_entry *)
                                       (disk + block_num[0] * EXT2_BLOCK_SIZE);
    
    /* Initialize various fields. */
    assert(inode_num != 0);
    dir_entry->inode = inode_num;
    dir_entry->rec_len = 12;
    dir_entry->name_len = 1;
    dir_entry->file_type = EXT2_FT_DIR;
    strncpy(dir_entry->name, ".", 1);

    dir_entry = (struct ext2_dir_entry *)((unsigned char *)dir_entry + 12);
    dir_entry->inode = parent_inode_num;
    dir_entry->rec_len = 1012;
    dir_entry->name_len = 2;
    dir_entry->file_type = EXT2_FT_DIR;
    strncpy(dir_entry->name, "..", 2);
}


/*
 * Initialize the file block i.e. write all the data to the disk.
 */
void init_file_block(int src_fd) {
    char buf[EXT2_BLOCK_SIZE];

    for (int i = 0; i < block_num_size; i++) {
        ssize_t res = read(src_fd, buf, EXT2_BLOCK_SIZE);
        if (res < 0) perror("read");

        unsigned char *dest = disk + block_num[i] * EXT2_BLOCK_SIZE;
        memcpy(dest, buf, (size_t)res);
    }
}

/*
 * Initialize a symbolic link's block i.e. write the absolute path of src to the disk.
 */
void init_symb_block(char *src_path) {
    char *dest = (char *)(disk + block_num[0] * EXT2_BLOCK_SIZE);
    strncpy(dest, src_path, strlen(src_path));
}

/*
 * Deallocate the dir_entry of a deleted file/dir.
 *
 * If we call this function, then it is guaranteed that we can find a dir_entry with the specified name.
 */
void dealloc_dir_block(int parent_num, char *name) {
    struct ext2_inode *parent_inode = inode_table + parent_num - 1;

    int block_list[128];
    memset(block_list, 0, 128 * sizeof(int));
    clear_block_list();
    dfs(parent_inode->i_block);

    int previous_len = 0;
    int len_sum = 0;
    int found = 0;
    struct ext2_dir_entry *dir_entry = NULL;
    int block_list_size = get_block_nums(block_list);

    /* Loop over all the data blocks. */
    for (int i = 0; i < block_list_size; i++) {
        dir_entry = (struct ext2_dir_entry *)
                    (disk + block_list[i] * EXT2_BLOCK_SIZE);
        previous_len = 0;
        len_sum = 0;

        /* Find the directory entry that contains the inode number. */
        while (len_sum != EXT2_BLOCK_SIZE) {
            if (strlen(name) == dir_entry->name_len &&
                strncmp(name, dir_entry->name, strlen(name)) == 0) {
                found = 1;
                break;
            }
            len_sum += dir_entry->rec_len;
            previous_len = dir_entry->rec_len;
            dir_entry = (struct ext2_dir_entry *)
                    ((unsigned char *) dir_entry + dir_entry->rec_len);
        }

        if (found) break;
    }

    if (!found) {
      /* This case can never happen if we already checked that
       * the file we want to delete exists. However, we will still
       * check it in order to ensure the robustness of our program. */
        fprintf(stderr, "No such file or directory.\n");
        exit(ENOENT);
    } else if (!previous_len) {
        dir_entry->inode = 0;
    } else {
        struct ext2_dir_entry *prev_dir_entry = (struct ext2_dir_entry *)
                ((unsigned char *)dir_entry - previous_len);
        prev_dir_entry->rec_len += dir_entry->rec_len;
    }
}


/*
 * Update the parent directory block by adding a new ext2_dir_entry.
 */
void update_parent_dir_block(int file_enable) {
    int occupied_space = sizeof(unsigned int) + sizeof(unsigned short) + 
                                            2 * sizeof(unsigned char);
    /* Expected rec_len of the added dir_entry. */ 
    int length = (int)(occupied_space + strlen(i_name));
    while (length % 4) {
        length++;
    }

    /* Find out the parent's last dir block num to append to. */
    struct ext2_inode *parent_inode = inode_table + parent_inode_num - 1;

    int block_list[128];
    memset(block_list, 0, 128 * sizeof(int));
    clear_block_list();
    dfs(parent_inode->i_block);
    int block_list_size = get_block_nums(block_list);
    int parent_block_num = block_list[block_list_size - 1];

    /* Find the last dir entry. */
    struct ext2_dir_entry *dir_entry = (struct ext2_dir_entry *)
                                       (disk + parent_block_num * EXT2_BLOCK_SIZE);
    int len_sum = 0;
    int curr_len = dir_entry->rec_len;
    while (len_sum + curr_len != EXT2_BLOCK_SIZE) {
        len_sum += curr_len;
        dir_entry = (struct ext2_dir_entry *)
                    ((unsigned char *)dir_entry + curr_len);
        curr_len = (int)(dir_entry->rec_len);
    }

    /* Previous dir_entry's actual rec_len. */
    int previous_len = occupied_space + dir_entry->name_len;
    while (previous_len % 4) {
        previous_len++;
    }

    int space_left = curr_len - previous_len;
    if (space_left < length) {
        /* Allocate a new block and update the contents in that block. */
        int new_block = set_bitmap(1);
        append_to_block(parent_inode_num, &new_block, 1);
        parent_inode->i_size += EXT2_BLOCK_SIZE;
        parent_inode->i_blocks += 2;

        struct ext2_dir_entry *new_entry = (struct ext2_dir_entry *)
                                           (disk + new_block * EXT2_BLOCK_SIZE);
        new_entry->inode = inode_num;
        new_entry->rec_len = EXT2_BLOCK_SIZE;
        new_entry->name_len = (unsigned char)strlen(i_name);

        if (file_enable == 1) new_entry->file_type = EXT2_FT_REG_FILE;
        else if (file_enable == 0) new_entry->file_type = EXT2_FT_DIR;
        else new_entry->file_type = EXT2_FT_SYMLINK;
        /* Exclude the null terminator when copying. */
        strncpy(new_entry->name, i_name, strlen(i_name));


        char new_name[strlen(i_name) + 1];
        strncpy(new_name, new_entry->name, strlen(i_name));
        new_name[strlen(i_name)] = '\0';
    } else {
        /* Update the rec_len of previous entry. */
        dir_entry->rec_len = (unsigned char)previous_len;

        /* Create a new entry at the end of the parent block. */
        dir_entry = (struct ext2_dir_entry *)
                    ((unsigned char *)dir_entry + previous_len);
        dir_entry->inode = inode_num;
        dir_entry->rec_len = (unsigned short)space_left;
        dir_entry->name_len = (unsigned char)strlen(i_name);

        if (file_enable == 1) dir_entry->file_type = EXT2_FT_REG_FILE;
        else if (file_enable == 0) dir_entry->file_type = EXT2_FT_DIR;
        else dir_entry->file_type = EXT2_FT_SYMLINK;
        /* Exclude the null terminator when copying. */
        strncpy(dir_entry->name, i_name, strlen(i_name));
    }

    clear_block_list();
}

/* Update the parent inode after creating a new dir/file. */
void update_parent_inode(int file_enable) {
    struct ext2_inode *parent_inode = inode_table + parent_inode_num - 1;
    if (file_enable == 0) parent_inode->i_links_count++;
}

/*
 * Add block_num's to the specified i_block.
 */
void append_to_block(int inode_num, int* block, int size) {
    struct ext2_inode *inode = inode_table + inode_num - 1;

    clear_block_list();
    dfs(inode->i_block);
    clear_block_list();

    int *block_arr;
    int write_idx = last_idx;
    if (parent_i_block) {
        /* We are already in the indirect block. */
        assert(size <= EXT2_BLOCK_SIZE / sizeof(unsigned int) - last_idx);
        block_arr = (int *)(disk + parent_i_block * EXT2_BLOCK_SIZE);
        for (int i = 0; i < size; i++) {
            block_arr[write_idx] = block[i];
            write_idx++;
        }
    } else {
        block_arr = (int *)inode->i_block;

        /* Flag indicate whether we have allocated an indirect block or not. */
        int indirect_allocated = 0;
        for (int i = 0; i < size; i++) {
            if (write_idx == 12 && !indirect_allocated) {
                /* under the simple assumption, we only have one indirect block where i == 12 */
                assert(indirect_block_num != 0);
                block_arr[write_idx] = indirect_block_num;

                block_arr = (int *)(disk + indirect_block_num * EXT2_BLOCK_SIZE);
                memset(block_arr, 0, EXT2_BLOCK_SIZE);

                write_idx = 0;
                indirect_allocated = 1;
            }
            block_arr[write_idx] = block[i];
            write_idx++;
        }
    }
}