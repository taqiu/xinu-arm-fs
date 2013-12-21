#include <kernel.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <bufpool.h>
#include <string.h>

#if FS
#include <fs.h>

#define valid_fd(x) ((x >= 0) && (x <= MAX_OPEN_FILE_NUM))

static struct fsystem fsd;
int dev0_numblocks;
int dev0_blocksize;
char *dev0_blocks;
int dev0 = 0;
struct fdesc filelist[MAX_OPEN_FILE_NUM];

int mkbsdev(int dev, int blocksize, int numblocks) {

  if (dev != 0) {
    printf("Unsupported device: %d\n", dev);
    return SYSERR;
  }

  if (blocksize != 0) {
    dev0_blocksize = blocksize;
  }
  else {
    dev0_blocksize = MDEV_BLOCK_SIZE;
  }

  if (numblocks != 0) {
    dev0_numblocks =  numblocks;
  }
  else {
    dev0_numblocks =  MDEV_NUM_BLOCKS;
  }

  if ((dev0_blocks = memget(dev0_numblocks * dev0_blocksize)) == SYSERR) {
    printf("mkbsdev memgetfailed\n");
    return SYSERR;
  }

  return OK;

}

int mkfs(int dev, int num_inodes) {
    int i, j;
    int bm_blk = 0;
    struct inode *inodes;
    struct inode in;
  
    if (dev == 0) {
        fsd.blocks = dev0_numblocks;
        fsd.blocksz = dev0_blocksize;
    }
    else {
        printf("Unsupported device\n");
        return SYSERR;
    }

    i = fsd.blocks;
    while ( (i % 8) != 0) {i++;}
    fsd.freemaskbytes = i / 8; 

    if ((fsd.freemask = memget(fsd.freemaskbytes)) == SYSERR) {
        printf("mkfs memget failed.\n");
        return SYSERR;
    }

    /* -1 the dirent */
    for (i = 0; i < DIRECTORY_SIZE; i ++) {
        fsd.root_dir.entry[i].inode_num = EMPTY_INODE;
    }

    /* zero the free mask */
    for(i=0;i<fsd.freemaskbytes;i++) {
        fsd.freemask[i] = '\0';
    }

    if ((inodes = memget(num_inodes*sizeof(struct inode))) == SYSERR) {
        printf("mkfs inode memget failed.\n");
        return SYSERR;
    }    

    /* write the fsystem block to block 0, mark block used */
    setmaskbit(0);
    bwrite(dev0, bm_blk, 0, &fsd, sizeof(struct fsystem));

    /* write the freemask in block 1, mark block used */
    setmaskbit(1);
    bwrite(dev0, bm_blk+1, 0, fsd.freemask, fsd.freemaskbytes);

   
    /* write inodes to filesystem */ 
    for (i=0; i<num_inodes; i++) {
        in.id = i;
        in.nlink = 0;
        in.device = dev0;
        in.size = 0;
        for (j=0; j<FILEBLOCKS; j++) {
            in.blocks[j] = 0;
        }
        put_inode_by_num(dev0, i, &in);
    }

    return OK;
}

int get_inode_by_num(int dev, int inode_number, struct inode *in) {
    int len = sizeof(struct inode);
    int block_num = inode_number / 3;
    int block_offset = inode_number % 3;
    if (bread(dev, INODE_BLOCK + block_num, len * block_offset, in, len) == SYSERR) {
        printf("Can not read inode\n");
    }

    return OK;
}

int put_inode_by_num(int dev, int inode_number, struct inode *in) {
    int len = sizeof(struct inode);
    int block_num = inode_number / 3;
    int block_offset = inode_number % 3;
    if (bwrite(dev, INODE_BLOCK + block_num, len * block_offset, in, len) == SYSERR) {
        printf("Can not wrtie inode\n");
    } else {
        setmaskbit(block_num);
    }
    return OK;
}

int mount(int dev) {
    int i, bm_blk = 0;
    if (dev != 0) {
        printf("Unsupported device\n");
        return SYSERR;
    }
 
    if (bread(dev0, bm_blk, 0, &fsd, sizeof(struct fsystem)) == SYSERR) {
        printf("Can not read file system block\n");
        return SYSERR;
    }

    if (bread(dev0, bm_blk+1, 0, fsd.freemask, fsd.freemaskbytes) == SYSERR) {
        printf("Can not read bit map\n");
        return SYSERR;
    }

    for (i = 0; i < MAX_OPEN_FILE_NUM; i ++) {
        filelist[i].state = N_FILE;
    }
    return OK;
}

int fcreate(char *filename, int mode) {
    int i, j;
    int entry_index = -1;
    int inode_index = -1;
    struct inode in;
    /* file name should not be empty */
    if (strcmp(filename, "") == 0) {
        printf("file name should not be empty\n");
        return SYSERR;
    }

    /* check duplication of fiel name */
    for (i = 0; i < DIRECTORY_SIZE; i ++) {
        if (strcmp(fsd.root_dir.entry[i].name, filename) == 0) {
            printf("file with the same name exists\n");
            return SYSERR;
        }
    }

    for (i = 0; i < DIRECTORY_SIZE; i ++) {
        if (fsd.root_dir.entry[i].inode_num == EMPTY_INODE) {
            entry_index = i;
            for (j = 0; j < INODE_SIZE; j ++) {
                if (get_inode_by_num(0, j, &in) == OK && in.nlink == 0) {
                    inode_index = j;
                    break;
                }
            }
            if (inode_index != -1) {
                strncpy(fsd.root_dir.entry[i].name, filename, strnlen(filename, FILENAMELEN + 1));
                fsd.root_dir.numentries ++;

                fsd.root_dir.entry[i].inode_num = j;
                in.nlink = 1;
                put_inode_by_num(0, j, &in);
                break;
            } else {
                entry_index = -1;
            }
        }
    }

    /* can not find empty inode */
    if (inode_index == -1) {
        printf("file entry is full\n");
        return SYSERR;
    }
    
    return OK;
}

int fopen(char *filename, int flags) {
    int i, j;
    for (i = 0; i < DIRECTORY_SIZE; i ++) {
        if (strcmp(fsd.root_dir.entry[i].name, filename) == 0) {
            for (j = 0; j < MAX_OPEN_FILE_NUM; j ++) {
                if (filelist[j].state == N_FILE) {
                    break;
                }
            }
            if (j >= MAX_OPEN_FILE_NUM) {
                printf("Open file list is full\n");
                return SYSERR;
            }
            if (get_inode_by_num(0, fsd.root_dir.entry[i].inode_num, &filelist[j].in) != OK) {
                printf("Can not get inode\n");
                return SYSERR;
            }
            filelist[j].state = flags;
            filelist[j].fptr = 0;
            return j;
        }
    }
    printf("Can not find the file\n");
    return SYSERR;
}

int fclose(int fd) {
    if (!valid_fd(fd)) {
        printf("Invalid file identifier\n");
        return SYSERR;
    }
    if (filelist[fd].state == N_FILE) {
        printf("File is not open\n");
        return SYSERR;
    }
    filelist[fd].state = N_FILE;
    filelist[fd].fptr = 0;
    if (put_inode_by_num(0, filelist[fd].in.id, &filelist[fd].in) == SYSERR) {
        printf("Can not update inode\n");
        return SYSERR;
    }
    return OK;
}

int fseek(int fd, int offset) {
    int file_size, tmptr;
    if (!valid_fd(fd)) {
        printf("Invalid file identifier\n");
        return SYSERR;
    }
    if (filelist[fd].state == N_FILE) {
        printf("File is not open\n");
        return SYSERR;
    }
    file_size = filelist[fd].in.size * fsd.blocksz;
    tmptr = filelist[fd].fptr + offset;
    if (tmptr < 0 || tmptr > file_size) {
        printf("Invalid offset\n");
        return SYSERR;
    }
    filelist[fd].fptr = tmptr;
    return OK;
}

int fread(int fd, void *buff, int nbytes) {
    int s_index, e_index, s_ptr, e_ptr, i;
    int offset, len, bufptr;
    if (!valid_fd(fd)) {
        printf("Invalid file identifier\n");
        return SYSERR;
    }
    if (filelist[fd].state == N_FILE) {
        printf("File is not open\n");
        return SYSERR;
    }
    if ((filelist[fd].state & O_READ != O_READ)) {
        printf("File opened without read permission\n");
        return SYSERR;
    }
    if (filelist[fd].fptr + nbytes > filelist[fd].in.size * fsd.blocksz) {
        printf("File size is smaller than request\n");
        return SYSERR;
    }
    // Start to read
    s_ptr = filelist[fd].fptr;
    e_ptr = filelist[fd].fptr + nbytes;
    s_index = s_ptr / fsd.blocksz;
    e_index = e_ptr / fsd.blocksz;
    bufptr = 0;
    for (i = s_index; i <= e_index; i ++) {
        offset = filelist[fd].fptr % fsd.blocksz;
        if (i == e_index) len = e_ptr % fsd.blocksz - offset;
        else len = fsd.blocksz - offset;
        if (bread(0, filelist[fd].in.blocks[i], offset, buff + bufptr, len) == SYSERR) {
            printf("Failed to read block %d\n", filelist[fd].in.blocks[i]);
            return SYSERR;
        }
        filelist[fd].fptr += len;
        bufptr += len;
    }

    return OK;
}

int fwrite(int fd, void *buf, int nbytes) {
    int s_ptr, e_ptr, s_index, e_index, i;
    int offset, len, bufptr, bptr;
    if (!valid_fd(fd)) {
        printf("Invalid file indentifier\n");
        return SYSERR;
    }
    if (filelist[fd].state == N_FILE) {
        printf("File is not open\n");
        return SYSERR;
    }
    if ((filelist[fd].state & O_WRITE != O_WRITE)) {
        printf("File opened withour write permission\n");
        return SYSERR;
    }
    // Start to write
    s_ptr = filelist[fd].fptr;
    e_ptr = filelist[fd].fptr + nbytes;
    s_index = s_ptr / fsd.blocksz;
    e_index = e_ptr / fsd.blocksz;
    bufptr = 0;
    for (i = s_index; i <= e_index; i ++) {
        offset = filelist[fd].fptr % fsd.blocksz;
        if (i == e_index) len = e_ptr % fsd.blocksz - offset;
        else len = fsd.blocksz - offset;
        if (i < filelist[fd].in.size) {
            // Overwrite
            if (bwrite(0, filelist[fd].in.blocks[i], offset, buf + bufptr, len) == SYSERR) {
                printf("Failed to write block %d\n", filelist[fd].in.blocks[i]);
                return SYSERR;
            }
            filelist[fd].fptr += len;
            bufptr += len;
        } else {
            bptr = get_free_block();
            if (bwrite(0, bptr, offset, buf + bufptr, len) == SYSERR) {
                printf("Failed to write block %d\n", bptr);
                return SYSERR;
            }
            filelist[fd].fptr += len;
            bufptr += len;
            setmaskbit(bptr);
            filelist[fd].in.blocks[filelist[fd].in.size] = bptr;
            filelist[fd].in.size ++;
        }
    }
}

int get_free_block() {
    int i;
    for (i = 0; i < fsd.blocks; i ++) {
        if (getmaskbit(i) == 1) return i;
    }
    return SYSERR;
}

int strcmp(char *str1, char *str2) {
    int len1 = strnlen(str1, FILENAMELEN + 1);
    int len2 = strnlen(str2, FILENAMELEN + 1);
    if (len1 != len2) {
        return -1;
    }
    if (strncmp(str1, str2, len1) == 0) {
        return 0;
    } else {
        return -1;
    }
}

void print_inodes() {
    struct inode node;
    int i;
    printf("Size of inode struct%d\n", sizeof(struct inode));
    for (i = 0; i < 16; i ++) {
        node.id = 1;
        get_inode_by_num(0, i, &node);
        printf("%d inode id: %d line: %d sinze: %d\n",i ,node.id, node.nlink, node.size);
    }
}

int 
bread(int dev, int block, int offset, void *buf, int len) {
  char *bbase;

  if (dev != 0) {
    printf("Unsupported device\n");
    return SYSERR;
  }
  if (offset >= dev0_blocksize) {
    printf("Bad offset\n");
    return SYSERR;
  }

  bbase = &dev0_blocks[block * dev0_blocksize];

  memcpy(buf, (bbase+offset), len);

  return OK;
}


int 
bwrite(int dev, int block, int offset, void * buf, int len) {
  char *bbase;

  if (dev != 0) {
    printf("Unsupported device\n");
    return SYSERR;
  }
  if (offset >= dev0_blocksize) {
    printf("Bad offset\n");
    return SYSERR;
  }

  bbase = &dev0_blocks[block * dev0_blocksize];

  memcpy((bbase+offset), buf, len);
  
  return OK;
}


/* specify the block number to be set in the mask */
int setmaskbit(int b) {
  int mbyte, mbit;
  mbyte = b / 8;
  mbit = b % 8;

  fsd.freemask[mbyte] |= (0x80 >> mbit);
}

/* specify the block number to be read in the mask */
int getmaskbit(int b) {
  int mbyte, mbit;
  mbyte = b / 8;
  mbit = b % 8;

  return( ( (fsd.freemask[mbyte] << mbit) & 0x80 ) >> 7);
}

/* specify the block number to be unset in the mask */
int clearmaskbit(int b) {
  int mbyte, mbit, invb;
  mbyte = b / 8;
  mbit = b % 8;

  invb = ~(0x80 >> mbit);
  invb &= 0xFF;

  fsd.freemask[mbyte] &= invb;
}

/* This is maybe a little overcomplicated since the first block is indicated in the
   high-order bit.  Shift the byte by j positions to make the match in bit7 (the 8th 
   bit) and then shift that value 7 times to the low-order bit to print.  Yes, it
   could be the other way...  */
void printfreemask(void) {
  int i,j;

  for (i=0; i < fsd.freemaskbytes; i++) {
    for (j=0; j < 8; j++) {
      printf("%d", ((fsd.freemask[i] << j) & 0x80) >> 7);
    }
    if ( (i % 8) == 7) {
      printf("\n");
    }
  }
  printf("\n");
}

#endif /* FS */
