/* Copyright 2012 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/* This is highly experimental code to test shared/dynamic images
 * with pnacl.
 * Currently, only x86-32 is tested.
 */

#include <unistd.h>
#include <stdlib.h>
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
extern void _dl_get_tls_static_info(int *static_tls_size,
                                    int *static_tls_align);
extern int __tls_get_addr();
/* ====================================================================== */
int mystrlen(const char* s) {
   int count = 0;
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

#define myprint(s) NACL_SYSCALL(write)(1, s, mystrlen(s))

__thread int tdata1 = 1;
__thread int tdata2 = 3;


ssize_t  write(int fd, const void* buf, size_t n)  {
  return NACL_SYSCALL(write)(fd, buf, n);
}

void exit(int ret) {
  NACL_SYSCALL(exit)(ret);
}

void __deregister_frame_info(const void *begin) {
  myprint("__deregister_frame_info\n");
}

void __register_frame_info(void *begin, void *ob) {
  myprint("__register_frame_info\n");
}


int main(int argc, char** argv, char** envp) {
  myprint("hello world\n");
  char buffer[9];

  myhextochar(tdata1, buffer);
  myprint(buffer);
  myprint(" expecting 1\n");

  myhextochar(tdata2, buffer);
  myprint(buffer);
  myprint(" expecting 3\n");

#if 0
  int static_tls_size;
  int static_tls_align;


  /* will be enabled soon */
  int x = (int) & __tls_get_addr;
  myhextochar(x, buffer);
  myprint(buffer);
  myprint("\n");
  _dl_get_tls_static_info (&static_tls_size, &static_tls_align);
#endif
  return 0;
}

char message_init[] = "init\n";

int __libc_csu_init (int argc, char **argv, char **envp) {
  write(1, message_init, sizeof(message_init));
  return 0;
}

char message_fini[] = "fini\n";
void __libc_csu_fini (void) {
  write(1, message_fini, sizeof(message_fini));
}

void __libc_start_main (int (*main) (int argc, char **argv, char **envp),
                        int argc, char **argv,
                         int (*init) (int argc, char **argv, char **envp),
                        void (*fini) (void),
                        void (*rtld_fini) (void),
                        void *stack_end) {

  exit(main(0, 0, 0));
}
