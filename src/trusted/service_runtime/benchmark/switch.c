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


#define _XOPEN_SOURCE 600
/* for posix_memalign from stdlib */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <malloc.h>
#include <errno.h>
#include <sys/poll.h>
#include <sys/mman.h>
#include <asm/ldt.h>
/*
 * null syscall; link against libsel.a
 */

#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/service_runtime/sel_rt.h"
#include "native_client/src/trusted/service_runtime/nacl_all_modules.h"
#include "native_client/src/trusted/service_runtime/nacl_app.h"
#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"
#include "native_client/src/trusted/service_runtime/nacl_ldt.h"
#include "native_client/src/trusted/service_runtime/nacl_syscalls.h"

extern struct NaClThreadContext *nacl_user[LDT_ENTRIES];
extern struct NaClThreadContext *nacl_sys[LDT_ENTRIES];
extern struct NaClAppThread *nacl_thread[LDT_ENTRIES];

#if 1
int num_syscalls = 100000000;
#else
int num_syscalls = 10;
#endif

int const kNumPagesMinus1 = (1 << (32-12))-1;

uintptr_t syscall_tbl;


/*
 * Only do system calls.
 */
void *app_thread(void) {
  int i, val;
  int (*sysp)();

  char hello[] = "Hello world\n";
  char bye[] = "Good bye cruel world\n";

  (void) write(1, hello, sizeof hello - 1);
  sysp = (int (*)()) (syscall_tbl + NACL_sys_null * NACL_INSTR_BLOCK_SIZE);
  for (i = 0; i < num_syscalls; ++i) {
    val = (*sysp)();
  }
  (void) write(1, bye, sizeof bye - 1);

  sysp = (int (*)()) (syscall_tbl +
                      NACL_sys_pthread_exit * NACL_INSTR_BLOCK_SIZE);
  (*sysp)(0);
  return (void *) 0xdeadbeef;
}


union {
  double  align;
  char    space[16 << 12];
} stack;

/*
 * easier for debugger; once main thread does futex, hard to find stack frames
 */
struct NaClApp        na;
struct NaClAppThread  nat;


void thread_sleep(int sec) {
  poll((struct pollfd *) 0, 0, 1000 * sec); /* poll takes mS */
}


void thread_block(void) {
  while (1) {
    poll((struct pollfd *) 0, 0, -1); /* return only if EINTR */
  }
}


int main(int ac, char **av) {
  int                   opt;
  int                   err;
  uint16_t              des_s, gs, cs;  /* ds=es=fs */
  Elf32_Phdr            ep;
  uint16_t              ldt_ix;

  NaClAllModulesInit();
  while ((opt = getopt(ac, av, "n:v")) != -1) {
    switch (opt) {
      case 'n':
        num_syscalls = strtoul(optarg, (char **) 0, 0);
        break;
      case 'v':
        NaClLogIncrVerbosity();
        break;
      default:
        fprintf(stderr, "Usage: switch [-n num_syscalls]\n");
        return -1;
    }
  }

  des_s = NaClLdtAllocateSelector(NACL_LDT_DESCRIPTOR_DATA,
                                  0,
                                  (void *) 0,
                                  kNumPagesMinus1);
#if 0
  /* don't know what values to use! */
  gs = NaClLdtAllocateSelector(NACL_LDT_DESCRIPTOR_DATA,
                               0,
                               (void *) 0,
                               kNumPagesMinus1);
#else
  gs = NaClGetGs();
#endif
  cs = NaClLdtAllocateSelector(NACL_LDT_DESCRIPTOR_CODE,
                               0,
                               (void *) 0,
                               kNumPagesMinus1);

  printf("des = 0x%x, gs = 0x%x, cs = 0x%x\n", des_s, gs, cs);

  /*
   * all three selectors give the entire address space, so should have
   * no effect.
   */
  ldt_ix = gs >> 3;

  printf("ldx_ix = 0x%x\n", ldt_ix);

  printf("nacl_thread[%d] = 0x%08x\n", ldt_ix, (uintptr_t) nacl_thread[ldt_ix]);
  printf("nacl_user[%d] = 0x%08x\n", ldt_ix, (uintptr_t) nacl_user[ldt_ix]);
  printf("nacl_sys[%d] = 0x%08x\n", ldt_ix, (uintptr_t) nacl_sys[ldt_ix]);

  /* fill in NaClApp object enough for code to work... */
  NaClAppCtor(&na);
#if 0
  na.mem_start = (uintptr_t) malloc(8 * 1024);
#else
  if (0 != (err = posix_memalign((void **) &na.mem_start, 4 << 12, 8 << 12))) {
    errno = err;
    perror("posix_memalign");
    return 1;
  }
  if (0 != mprotect((void *) na.mem_start,
                    8 << 12,
                    PROT_READ | PROT_EXEC | PROT_WRITE)) {
    perror("mprotect");
    return 1;
  }
#endif
  na.phdrs = &ep;

  na.xlate_base = 0;

  /* generate trampoline thunk */
  NaClLoadTrampoline(&na);
  NaClLoadSpringboard(&na);

  syscall_tbl = (uintptr_t) na.mem_start;
  /* start of trampoline */

  printf("initializing main thread\n");
  printf("ds = es = ss = 0x%x\n", des_s);
  printf("gs = 0x%x\n", gs);
  printf("cs = 0x%x\n", cs);

  NaClAppPrepareToLaunch(&na);
  printf("after prepare to launch:\n");
  printf("ds = es = ss = 0x%x\n", des_s);
  printf("gs = 0x%x\n", gs);
  printf("cs = 0x%x\n", cs);

  na.code_seg_sel = cs;
  na.data_seg_sel = des_s;

  printf("na.code_seg_sel = 0x%02x\n", na.code_seg_sel);
  printf("na.data_seg_sel = 0x%02x\n", na.data_seg_sel);
  printf("&na.code_seg_sel = 0x%08x\n", (uintptr_t) &na.code_seg_sel);
  printf("&na.data_seg_sel = 0x%08x\n", (uintptr_t) &na.data_seg_sel);
  printf("&nat = 0x%08x\n", (uintptr_t) &nat);
  printf("&na = 0x%08x\n", (uintptr_t) &na);

  NaClAppThreadCtor(&nat,
                    &na,
                    (uintptr_t) app_thread,
                    ((uintptr_t) (&stack + 1)) - 128,
                    gs);

  printf("waiting for app thread to finish\n");

  NaClMutexLock(&nat.mu);
  while (nat.state != NACL_APP_THREAD_DEAD) {
    NaClCondVarWait(&nat.cv, &nat.mu);
  }
  NaClMutexUnlock(&nat.mu);

  printf("all done\n");
  printf("freeing posix_memaligned memory\n");
  free((void *) na.mem_start);
  na.mem_start = 0;
  printf("calling dtor\n");
  NaClAppDtor(&na);
  printf("calling fini\n");
  NaClAllModulesFini();
  printf("quitting\n");

  return 0;
}
