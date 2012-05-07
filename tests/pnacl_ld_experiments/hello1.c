/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* This is highly experimental code to test shared/dynamic images
 * with pnacl.
 * Currently, only x86-32 is tested.
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include <bits/nacl_syscalls.h>

/* ====================================================================== */
#define NACL_INSTR_BLOCK_SHIFT         5
#define NACL_PAGESHIFT                12
#define NACL_SYSCALL_START_ADDR       (16 << NACL_PAGESHIFT)
#define NACL_SYSCALL_ADDR(syscall_number)                               \
  (NACL_SYSCALL_START_ADDR + (syscall_number << NACL_INSTR_BLOCK_SHIFT))

#define NACL_SYSCALL(s) ((TYPE_nacl_ ## s) NACL_SYSCALL_ADDR(NACL_sys_ ## s))

/* ====================================================================== */
/* proto-types for nacl syscalls */

typedef int (*TYPE_nacl_write) (int desc, void const *buf, int count);
typedef void (*TYPE_nacl_null) (void);
typedef void (*TYPE_nacl_exit) (int status);

/* ====================================================================== */
/* proto-types for functions  inside ld.so */
#define REGPARM(n) __attribute__((regparm(n)))

extern REGPARM(2) void _dl_get_tls_static_info(int *static_tls_size,
                                               int *static_tls_align);

extern void _dl_debug_state();

extern int fortytwo();

/* This does not seem to have any effect - left for reference
 */
#define EXPORT __attribute__ ((visibility("default"),externally_visible))
/* ====================================================================== */
/* poor man's io */
/* ====================================================================== */
size_t mystrlen(const char* s) {
   unsigned int count = 0;
   while (*s++) ++count;
   return count;
}


void myhextochar(int n, char buffer[9]) {
  int i;
  buffer[8] = 0;

  for (i = 0; i < 8; ++i) {
    int nibble = 0xf & (n >> (4 * (7 - i)));
    if (nibble <= 9) {
      buffer[i] = nibble + '0';
    } else {
      buffer[i] = nibble - 10 + 'A';
     }
  }
}

ssize_t mywrite(int fd, const void* buf, size_t n)  {
  return NACL_SYSCALL(write)(fd, buf, n);
}

#define myprint(s) mywrite(1, s, mystrlen(s))

/* ====================================================================== */
/* stub functions */
/* ====================================================================== */
void __deregister_frame_info(const void *begin) {
  myprint("STUB __deregister_frame_info\n");
}

void __register_frame_info(void *begin, void *ob) {
  myprint("STUB __register_frame_info\n");
}

/* ====================================================================== */
/* dummy functions */
/* ====================================================================== */
void __local_lock_acquire() {
}

void __local_lock_acquire_recursive() {
}

void __local_lock_close_recursive() {
}

void __local_lock_init_recursive() {
}

void __local_lock_release() {
}

void __local_lock_release_recursive() {
}

char* getlogin() {
  return 0;
}

void issetugid() {
}

void sigprocmask() {
}

/* ====================================================================== */
__thread int tdata1 = 1;
__thread int tdata2 = 3;


int main(int argc, char** argv, char** envp) {
  myprint("hello world\n");
  char buffer[9];

  /* ======================================== */
  /* this is usually called by either
   * src/untrusted/nacl/pthread_initialize_minimal.c
   * but we do not want any of the tls setup which goes with it
   */
  __newlib_thread_init();
  puts("libc call\n");
  printf("another libc call %p\n", buffer);
  printf("another libc call %p\n", &tdata1);

  time_t rawtime;
  time(&rawtime);
  printf("The current local time is: %s\n", ctime(&rawtime));


  /* ======================================== */
  /* tls initialization test */
  myhextochar(tdata1, buffer);
  myprint(buffer);
  myprint(" expecting 1\n");

  myhextochar(tdata2, buffer);
  myprint(buffer);
  myprint(" expecting 3\n");

  /* ======================================== */
  /* call into another .so (and back) */
  int x = fortytwo();
  myhextochar(x, buffer);
  myprint(buffer);
  myprint(" expecting 42\n");

  /* ======================================== */
  /* trivial call into ld.so */
  _dl_debug_state();

  /* ======================================== */
  /* non-trivial call into ld.so */
  int static_tls_size;
  int static_tls_align;
  _dl_get_tls_static_info (&static_tls_size, &static_tls_align);
  myprint("tls size and alignment\n");
  myhextochar(static_tls_size, buffer);
  myprint(buffer);
  myprint("\n");
  myhextochar(static_tls_align, buffer);
  myprint(buffer);
  myprint("\n");

  myprint("exit\n");
  return 0;
}

