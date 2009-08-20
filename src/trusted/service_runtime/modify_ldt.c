/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Test code for exercising the modify_ldt function.
 */
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <asm/ldt.h>


/* @IGNORE_LINES_FOR_CODE_HYGIENE[2] */
extern int modify_ldt(int, void *, unsigned long);
extern int etext;


void hex_dump(uint8_t *buf, int size) {
  int i;
  int sep;
  for (i = 0; i < size; ++i) {
    sep = ' ';
    if (!(i & 0x3)) sep = '\t';
    if (!(i & 0xf)) sep = '\n';
    printf("%c%02x", sep, buf[i]);
  }
  printf("\n");
}

unsigned short getds(void) {
  unsigned short  ds;

  asm("mov %%ds, %0"
      : "=r" (ds)
      : );

  return ds;
}


void  setds(unsigned short ds) {
  asm("movw %0, %%ds"
      : : "r" (ds));
}


void  print_segreg(unsigned short seg) {
  printf("%02x: index %d (%x), %s, rpl %d\n",
         seg,
         (seg >> 3),
         (seg >> 3),
         (seg & 0x4) ? "LDT" : "GDT",
         (seg & 0x3));
}


void  print_segdesc(uint8_t  *ptr) {
  uint32_t    base_addr;
  uint32_t    seg_limit;  /* 20 bits */
  uint32_t    v;
  int         g, db, p, dpl, s, type;

  v = *ptr++;
  seg_limit = v;
  v = *ptr++;
  seg_limit |= (v << 8);
  v = *ptr++;
  base_addr = v;
  v = *ptr++;
  base_addr |= (v << 8);
  v = *ptr++;
  base_addr |= (v << 16);
  v = *ptr++;
  type = v & 0xf;
  s = (v >> 4) & 1;
  dpl = (v >> 5) & 0x3;
  p = (v>>7) & 1;
  v = *ptr++;
  seg_limit |= (v & 0xf) << 16;
  db = (v >> 6) & 1;
  g = (v >> 7) & 1;
  v = *ptr++;
  base_addr |= (v << 24);

  printf("base addr: 0x%08x\n", base_addr);
  printf("limit:     0x%05x\n", seg_limit);
  printf("g %d, db %d, p %d, dpl %x, s %d, type %x\n",
         g, db, p, dpl, s, type);
}


int main(int ac, char **av) {
  int               opt;

  intptr_t          base_addr = 0;  /* 0x08048000; */
  int               num_pages = -1;
  intptr_t          addr;
  uint8_t           buf[LDT_ENTRIES * LDT_ENTRY_SIZE];
  int               actual;
  struct user_desc  ud;
  uint16_t          ds, nds;
  char const        *fmt = "mem[0x%08x] = %02x\n";
  char              ch;

  while ((opt = getopt(ac, av, "a:b:p:")) != -1) {
    switch (opt) {
      case 'a':
        addr = strtoul(optarg, (char **) 0, 0);
        break;
      case 'b':
        base_addr = strtoul(optarg, (char **) 0, 0);
        break;
      case 'p':
        num_pages = strtoul(optarg, (char **) 0, 0);
        break;
      default:
        fprintf(stderr,
                "Usage: modify_ldt "
                "[-a addr-to-read] [-b base_addr] [-p num_pages]\n");
        return 1;
    }
  }

  printf("main:            0x%08x\n", (intptr_t) main);
  printf("etext:           0x%08x\n", (intptr_t) &etext);
  printf("aprox stack loc: 0x%08x\n", (intptr_t) &opt);
  printf("fmt loc:         0x%08x\n", (intptr_t) fmt);

  printf("ds = %x\n", getds());
  print_segreg(getds());

  actual = modify_ldt(0, buf, sizeof buf);
  if (-1 == actual) {
    perror("modify_ldt");
    exit(1);
  }
  printf("LDT size %d (0x%x)\n", actual, actual);
  hex_dump(buf, actual);

  printf("ds = %x\n", getds());
  print_segreg(getds());

  ud.entry_number = 0;
  ud.base_addr = base_addr;
  if (-1 == num_pages) {
    num_pages = ((1 << 20)-1) - base_addr / (4 << 10);
  }

  if (num_pages != (0xfffff & num_pages)) {
    fprintf(stderr, "WARNING: num_pages overflows\n");
  }
  ud.limit = num_pages;
  ud.seg_32bit = 1;
  ud.contents = MODIFY_LDT_CONTENTS_DATA;
  ud.read_exec_only = 0;
  ud.limit_in_pages = 1;
  ud.seg_not_present = 0;
  ud.useable = 1;

  printf("base addr: 0x%08x\n", base_addr);
  printf("num pages: 0x%08x\n", num_pages);
  printf("end addr:  0x%08x\n", base_addr + ((num_pages+1) << 12));

  actual = modify_ldt(1, &ud, sizeof ud);
  printf("returned %d\n", actual);

  ds = getds();
  setds((ud.entry_number << 3) | (1<<2) | (0x3));
  nds = getds();

  if (0 != addr) {
    unsigned char   *p = (unsigned char *) addr;

    ch = *p;
  }

  setds(ds);

  if (0 != addr) {
    printf(fmt, addr, ch);
  }

  printf("while ds modified:\n");
  print_segreg(nds);

  actual = modify_ldt(0, buf, sizeof buf);
  if (-1 == actual) {
    perror("modify_ldt");
    exit(1);
  }
  printf("LDT size %d (0x%x)\n", actual, actual);
  print_segdesc(buf);
  hex_dump(buf, 10 * LDT_ENTRY_SIZE);

  return 0;
}
