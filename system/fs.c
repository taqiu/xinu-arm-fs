#include <kernel.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <bufpool.h>

#if FS
#include <fs.h>

static struct fsystem fsd;
int dev0_numblocks;
int dev0_blocksize;
char *dev0_blocks;
int dev0 = 0;

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
    int bm_blk = 0;
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
                strcpy(fsd.root_dir.entry[i].name, filename);
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
