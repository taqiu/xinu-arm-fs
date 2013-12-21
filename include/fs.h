#ifndef FS_H
#define FS_H

#define FILENAMELEN 16
#define FILEBLOCKS 32
#define DIRECTORY_SIZE 16

#define MDEV_BLOCK_SIZE 512
#define MDEV_NUM_BLOCKS 512

#define INODE_BLOCK 2
#define EMPTY_INODE -1
#define INODE_SIZE 16
#define MAX_OPEN_FILE_NUM 16

#define N_FILE -1
#define O_READ 1
#define O_WRITE 2

struct inode {
  int id;
  int device;
  int nlink;
  int size;
  int blocks[FILEBLOCKS];
};

struct fdesc {
  int state;
  struct inode in;
  int fptr;
};

struct dirent {
  int inode_num;
  char name[FILENAMELEN];
};

struct directory {
  int numentries;
  struct dirent entry[DIRECTORY_SIZE];
};

struct fsystem {
  int blocks;
  int blocksz;
  int freemaskbytes;
  char *freemask;
  struct directory root_dir;
};

int fopen(char *filename, int flags);
int fclose(int fd);
int fcreat(char *filename, int mode);
int fseek(int fd, int offset);
int fread(int fd, void *buf, int nbytes);
int fwrite(int fd, void *buf, int nbytes);

/* filesystem functions */
int mkfs(int dev, int num_inodes);
int mount(int dev);

/*
  Block Store
  bread, bwrite,
  bput, bget write entire blocks (macro with offset=0, len=blocksize)
 */
int mkbsdev(int dev, int blocksize, int numblocks);
int bread(int bsdev, int block, int offset, void *buf, int len);
int bwrite(int bsdev, int block, int offset, void * buf, int len);

/* debugging functions */
void printfreemask(void);
int setmaskbit(int b);
int getmaskbit(int b);
int get_inode_by_num(int dev, int inode_number, struct inode *in);
int put_inode_by_num(int dev, int inode_number, struct inode *in);

/* test & help functions*/
void print_inodes();
int get_free_block();
int strcmp(char *str1, char *str2);
#endif /* FS_H */
