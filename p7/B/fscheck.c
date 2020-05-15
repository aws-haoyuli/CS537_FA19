#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdlib.h>
#include "fs.h"

void perr(char *msg) {
    fprintf(stderr, "ERROR: %s\n", msg);
    exit(1);
}

int main(int argc, char *argv[]) {
    // open file img
    int fd_fs = open(argv[1], O_RDONLY);
    if (fd_fs < 0) {
        perr("image not found.");
    }

    // get statistics of img, mmap and super block
    struct stat fs_stat;
    fstat(fd_fs, &fs_stat);
    void *fs_ptr = mmap(NULL, fs_stat.st_size, PROT_READ, MAP_PRIVATE, fd_fs,
                        0);
    struct superblock *sb = (struct superblock *) (fs_ptr + BSIZE);
    const int sb_size = sb->size;
    const int sb_inodes = sb->ninodes;

    // get first inode
    struct dinode *curr_inode = (struct dinode *) (fs_ptr +
                                                   (sb->inodestart) * BSIZE);

    // transform bit map from blocks to an array
    short *bmap = (short *)malloc(sb_size * sizeof(short));
    memset(bmap, 0, sb->size * sizeof(short));
    int cnt = 0;
    char *p_bmap = fs_ptr + (sb->bmapstart) * BSIZE;
    for (int i = 0; i < sb->size; i++) {
        int mask = 1 << cnt;
        bmap[i] = mask & *p_bmap;
        if (cnt >= 7) {
            cnt = -1;
            p_bmap++;
        }
        cnt++;
    }
    // calculate # blocks of bit map
    int num_blk_map = (sb->size + BPB - 1) / BPB;

    // record the use of data blocks，reference of inodes and use
    // of inodes
    short *blk_used = (short *)malloc(sb_size * sizeof(short));
    short *ind_ref = (short *)malloc(sb_inodes * sizeof(short));
    short *ind_used = (short *)malloc(sb_inodes * sizeof(short));
    // short blk_used[sb_size];
    // short ind_ref[sb_inodes];
    // short ind_used[sb_inodes];
    memset(blk_used, 0, sb->size * sizeof(short));
    memset(ind_ref, 0, sb->ninodes * sizeof(short));
    memset(ind_used, 0, sb->ninodes * sizeof(short));


    // begin check
    for (int i = 0; i < sb->ninodes; i++) {
        // Each inode is either unallocated or one of the valid types
        // (T_FILE, T_DIR, T_DEV)
        if (curr_inode->type > 3 || curr_inode->type < 0) {
            perr("bad inode.");
        }

        // For in-use inodes, the size of the file is in a valid range
        // given the number of valid datablocks. ERROR: bad size in inode.
        int sum = 0;
        if (curr_inode->type != 0) {
            // traverse all DIRECT blocks
            for (int j = 0; j < NDIRECT; j++) {
                if (curr_inode->addrs[j] != 0) {
                    sum += BSIZE;
                    blk_used[curr_inode->addrs[j]] = 1;
                    // Each data block that is in use (pointed to by an
                    // allocated inode), is also marked in use in the bitmap.
                    if (bmap[curr_inode->addrs[j]] == 0) {
                        perr("bitmap marks data free but data block used "
                             "by inode.");
                    }
                }
            }
            ind_used[i] = 1;

            if (curr_inode->addrs[NDIRECT] != 0) {
                // signal the NDIRECT block used
                blk_used[curr_inode->addrs[NDIRECT]] = 1;
                // traverse all INDIRECT blocks
                unsigned int *p_indire_blk = (unsigned int *) (fs_ptr +
                        (BSIZE * (curr_inode->addrs[NDIRECT])));
                for (int j = 0; j < NINDIRECT; j++) {
                    if ((*p_indire_blk) != 0) {
                        sum += BSIZE;
                        blk_used[*p_indire_blk] = 1;
                        // Each data block that is in use (pointed to by an
                        // allocated inode), is also marked in use in the
                        // bitmap.
                        if (bmap[*p_indire_blk] == 0) {
                            perr("bitmap marks data free but data block used "
                                 "by inode.");
                        }
                    }
                    p_indire_blk++;
                }
            }

            // fprintf(stdout,"inode num = %d, sum = %d, size = %d\n", i,
            // sum-BSIZE, curr_inode -> size);
            if ((curr_inode->size > sum) ||
                ((int) (curr_inode->size) <= (sum - BSIZE))) {
                // if(curr_inode -> size > sum){
                //     fprintf(stdout,"1");
                // }
                // if((curr_inode -> size) <= (sum - BSIZE)){
                //     fprintf(stdout,"2");
                // }
                perr("bad size in inode.");
            }
        }

        // Root directory exists, and it is inode number 1.
        if (i == ROOTINO) {
            if (curr_inode->type != 1) {
                perr("root directory does not exist.");
            }
            // check . and .. directory
            struct dirent *dir = (struct dirent *) (fs_ptr +
                    (BSIZE * curr_inode->addrs[0]));
            if (dir->inum != 1 || (dir + 1)->inum != 1) {
                perr("root directory does not exist.");
            }
        }

        // The . entry in each directory refers to the correct inode.
        if (curr_inode->type == 1) {
            // check . directory
            struct dirent *dir_etr = (struct dirent *) (fs_ptr +
                    (BSIZE * curr_inode->addrs[0]));
            if (dir_etr->inum != i) {
                perr("current directory mismatch.");
            }

            // check direct data block, assume there all are directories
            // in a block
            for (int j = 0; j < NDIRECT; j++) {
                // if addrs[j] is valid
                if (curr_inode->addrs[j] != 0) {
                    // each block correspondes to BSIZE/sizeof(struct dirent)
                    // directory entries
                    struct dirent *dir_curr = (struct dirent *) (fs_ptr +
                            (BSIZE * curr_inode->addrs[j]));
                    for (int k = 0; k < (BSIZE / sizeof(struct dirent)); k++) {
                        if (dir_curr->inum != 0) {
                            ind_ref[dir_curr->inum] = 1;
                        }
                        dir_curr++;
                    }
                }
            }

            // if addrs[NDIRECT] is valid
            if (curr_inode->addrs[NDIRECT] != 0) {
                // check indirect data block
                unsigned int *indirect_ptr = (unsigned int *) (fs_ptr +
                        (BSIZE * (curr_inode->addrs[NDIRECT])));
                for (int j = 0; j < NINDIRECT; j++) {
                    // if *indirect_ptr is valid
                    if ((*indirect_ptr) != 0) {
                        struct dirent *dir_curr = (struct dirent *)
                                (fs_ptr + ((*indirect_ptr) * BSIZE));
                        for (int k = 0;
                             k < (BSIZE / sizeof(struct dirent)); k++) {
                            if (dir_curr->inum != 0) {
                                ind_ref[dir_curr->inum] = 1;
                            }
                            dir_curr++;
                        }
                    }
                    indirect_ptr++;
                }
            }
        }
        curr_inode++;
    }

    // For data blocks marked in-use in the bitmap,
    // actually is in-use in an inode or indirect block somewhere.
    for (int i = sb->bmapstart + num_blk_map; i < sb->size; i++) {
        if (bmap[i] != 0 && blk_used[i] == 0) {
            perr("bitmap marks data block in use but not used.");
        }
    }

    // Multi-Structure Checks
    for (int i = 0; i < sb->ninodes; i++) {
        // For inodes marked used in inode table,
        // must be referred to in at least one directory.
        if (ind_used[i] == 1 && ind_ref[i] == 0) {
            perr("inode marked in use but not found in a directory.");
        }
        // For inode numbers referred to in a valid directory,
        // actually marked in use in inode table.
        if (ind_used[i] == 0 && ind_ref[i] == 1) {
            perr("inode marked free but referred to in directory.");
        }
    }

    free(bmap);
    free(blk_used);
    free(ind_ref);
    free(ind_used);
    exit(0);
}
