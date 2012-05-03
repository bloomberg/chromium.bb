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

extern void _dl_debug_state();

extern int __tls_get_addr();

extern int ___tls_get_addr();

extern int fortytwo();

/* This does not seem to have any effect - left for reference
 */
#define EXPORT __attribute__ ((visibility("default"),externally_visible))

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

__thread int tdata1 = 1;
__thread int tdata2 = 3;


int mywrite(int fd, const void* buf, int n)  {
  return NACL_SYSCALL(write)(fd, buf, n);
}

#define myprint(s) mywrite(1, s, mystrlen(s))



void exit(int ret) {
  NACL_SYSCALL(exit)(ret);
}

void __deregister_frame_info(const void *begin) {
  myprint("DUMMY __deregister_frame_info\n");
}

void __register_frame_info(void *begin, void *ob) {
  myprint("DUMMY __register_frame_info\n");
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

  /* call into another .so (and back) */
  int x = fortytwo();
  myhextochar(x, buffer);
  myprint(buffer);
  myprint(" expecting 42\n");

  /* call into ld.so */
  _dl_debug_state();

#if 0
  /* will be enabled soon */
  int tls_fun_addr = (int) & __tls_get_addr;
  myhextochar(tls_fun_addr, buffer);
  myprint(buffer);
  myprint("\n");

  int tls_addr = ___tls_get_addr();
  myhextochar(tls_addr, buffer);
  myprint(buffer);
  myprint("\n");
#endif

#if 0
  /* will be enabled soon */
  int static_tls_size;
  int static_tls_align;
  _dl_get_tls_static_info (&static_tls_size, &static_tls_align);
#endif
  return 0;
}

char message_init[] = "init\n";

int __libc_csu_init (int argc, char **argv, char **envp) {
  mywrite(1, message_init, sizeof(message_init));
  return 0;
}

char message_fini[] = "fini\n";
void __libc_csu_fini (void) {
  mywrite(1, message_fini, sizeof(message_fini));
}

void __libc_start_main (int (*main) (int argc, char **argv, char **envp),
                        int argc, char **argv,
                         int (*init) (int argc, char **argv, char **envp),
                        void (*fini) (void),
                        void (*rtld_fini) (void),
                        void *stack_end) {

  exit(main(0, 0, 0));
}
