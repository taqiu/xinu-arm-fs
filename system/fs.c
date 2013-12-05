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

  if ( (dev0_blocks = memget(dev0_numblocks * dev0_blocksize)) == SYSERR) {
    printf("mkbsdev memgetfailed\n");
    return SYSERR;
  }

  return OK;

}

int mkfs(int dev) {
  int i;
  int bm_blk = 0;
  
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

 /* zero the free mask */
 for(i=0;i<fsd.freemaskbytes;i++) {
   fsd.freemask[i] = '\0';
 }

 /* write the fsystem block to block 0, mark block used */
 setmaskbit(0);
 bwrite(dev0, bm_blk, 0, &fsd, sizeof(struct fsystem));

 /* write the freemask in block 0, mark block used */
 setmaskbit(1);
 bwrite(dev0, bm_blk, 0, fsd.freemask, fsd.freemaskbytes);
 
 return 1;
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
