/* mmu.c --- 
 * 
 * Filename: mmu.c
 * Description: 
 * Author: Bryce Himebaugh
 * Maintainer: 
 * Created: Tue Nov 12 08:52:10 2013
 * Last-Updated: 
 *           By: 
 *     Update #: 0
 * Keywords: 
 * Compatibility: 
 * 
 */

/* Commentary: 
 * 
 * 
 * 
 */

/* Change log:
 * 
 * 
 */

/* Copyright (c) 2004-2007 The Trustees of Indiana University and 
 * Indiana University Research and Technology Corporation.  
 * 
 * All rights reserved. 
 * 
 * Additional copyrights may follow 
 */

/* Code: */

#include <stdio.h> 

/* void setup_mmu(void) { */
/*   kprintf("\r\ninside setup mmu routine\r\n"); */
/* } */


#define NUM_PAGE_TABLE_ENTRIES 4096 /* 1 entry per 1MB, so this covers 4G address space */
#define CACHE_DISABLED    0x12
#define SDRAM_START       0x80000000
#define SDRAM_END         0x8fffffff
#define CACHE_WRITEBACK   0x1e

inline void setup_mmu(void) {
  static unsigned int __attribute__((aligned(16384)))page_table[NUM_PAGE_TABLE_ENTRIES];
  int i;
  unsigned int reg;
  
  kprintf("\r\ninside setup mmu routine\r\n"); 
  /* Set up an identity-mapping for all 4GB, rw for everyone */
  for (i = 0; i < NUM_PAGE_TABLE_ENTRIES; i++)
    page_table[i] = i << 20 | (3 << 10) | CACHE_DISABLED;
  /* Then, enable cacheable and bufferable for RAM only */
  for (i = SDRAM_START >> 20; i < SDRAM_END >> 20; i++) {
    page_table[i] = (i%16) << 20 | (3 << 10) | CACHE_WRITEBACK;
    kprintf("i=0x%x, value=0x%x\r\n",i,page_table[i]);
  }
  
  /* Copy the page table address to cp15 */
  asm volatile("mcr p15, 0, %0, c2, c0, 0": : "r" (page_table) : "memory");
  /* Set the access control to all-supervisor */
  asm volatile("mcr p15, 0, %0, c3, c0, 0" : : "r" (~0));
  /* Enable the MMU */
  asm("mrc p15, 0, %0, c1, c0, 0" : "=r" (reg) : : "cc");
  reg|=0x1;
  asm volatile("mcr p15, 0, %0, c1, c0, 0" : : "r" (reg) : "cc");
}



/* mmu.c ends here */
