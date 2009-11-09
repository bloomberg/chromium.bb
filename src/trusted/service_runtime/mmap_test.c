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

#include "native_client/src/include/portability.h"

#include <stdio.h>
#include <string.h>

#include "native_client/src/trusted/service_runtime/include/sys/fcntl.h"
#include "native_client/src/trusted/service_runtime/include/sys/errno.h"
#include "native_client/src/trusted/service_runtime/include/sys/mman.h"

#include "native_client/src/trusted/gio/gio.h"
#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"
#include "native_client/src/trusted/service_runtime/nacl_assert.h"
#include "native_client/src/trusted/service_runtime/nacl_syscall_common.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/desc/nacl_desc_effector_ldr.h"

/*
 * This redefines main when SDL is used to allow SDL to keep the main loop.
 */
#if defined(HAVE_SDL)
# include <SDL.h>
#endif


/* Based on NaClAppThreadCtor() */
static void InitThread(struct NaClApp *nap, struct NaClAppThread *natp) {
  struct NaClDescEffectorLdr *effp;

  memset(natp, 0xff, sizeof(*natp));

  natp->nap = nap;

  if (!NaClMutexCtor(&natp->mu)) {
    ASSERT(0);
  }
  if (!NaClCondVarCtor(&natp->cv)) {
    ASSERT(0);
  }

  natp->is_privileged = 0;
  natp->refcount = 1;

  effp = (struct NaClDescEffectorLdr *) malloc(sizeof *effp);
  ASSERT_NE(effp, NULL);

  if (!NaClDescEffectorLdrCtor(effp, natp)) {
    ASSERT(0);
  }
  natp->effp = (struct NaClDescEffector *) effp;
}


int main(int argc, char **argv) {
  char *nacl_file;
  struct GioMemoryFileSnapshot gf;
  struct NaClApp state;
  struct NaClAppThread nat, *natp = &nat;
  int errcode;
  uintptr_t initial_addr;
  uintptr_t addr;
  struct NaClVmmap *mem_map;
  char *nacl_verbosity = getenv("NACLVERBOSITY");

  if (argc < 2) {
    printf("No nexe file!\n\nFAIL\n");
  }
  nacl_file = argv[1];

  NaClLogModuleInit();

  NaClLogSetVerbosity((NULL == nacl_verbosity)
                      ? 0
                      : strtol(nacl_verbosity, (char **) 0, 0));

  errcode = GioMemoryFileSnapshotCtor(&gf, nacl_file);
  ASSERT_NE(errcode, 0);
  errcode = NaClAppCtor(&state);
  ASSERT_NE(errcode, 0);
  errcode = NaClAppLoadFile((struct Gio *) &gf,
                            &state,
                            NACL_ABI_MISMATCH_OPTION_ABORT);
  ASSERT_EQ(errcode, 0);

  InitThread(&state, natp);

  /* Allocate range */
  addr = NaClSysMmap(natp, 0, 3 * NACL_MAP_PAGESIZE,
                     NACL_ABI_PROT_READ | NACL_ABI_PROT_WRITE,
                     NACL_ABI_MAP_ANONYMOUS | NACL_ABI_MAP_PRIVATE,
                     -1, 0);
  printf("addr=0x%"PRIxPTR"\n", addr);
  initial_addr = addr;

  /* Map to overwrite the start of the previously allocated range */
  addr = NaClSysMmap(natp, (void *) initial_addr, 2 * NACL_MAP_PAGESIZE,
                     NACL_ABI_PROT_READ | NACL_ABI_PROT_WRITE,
                     NACL_ABI_MAP_ANONYMOUS | NACL_ABI_MAP_PRIVATE
                     | NACL_ABI_MAP_FIXED,
                     -1, 0);
  printf("addr=0x%"PRIxPTR"\n", addr);
  ASSERT_EQ(addr, initial_addr);

  /* Allocate new page */
  addr = NaClSysMmap(natp, 0, NACL_MAP_PAGESIZE,
                     NACL_ABI_PROT_READ | NACL_ABI_PROT_WRITE,
                     NACL_ABI_MAP_ANONYMOUS | NACL_ABI_MAP_PRIVATE,
                     -1, 0);
  printf("addr=0x%"PRIxPTR"\n", addr);
  /*
   * Our allocation strategy is to scan down from stack.  This is an
   * implementation detail and not part of the guaranteed semantics,
   * but it is good to test that what we expect of our implementation
   * didn't change.
   */
  ASSERT_EQ_MSG(addr, initial_addr - NACL_MAP_PAGESIZE,
                "Allocation strategy changed!");

  mem_map = &state.mem_map;
  NaClVmmapMakeSorted(mem_map);
  ASSERT_EQ(mem_map->nvalid, 7);
  NaClVmmapDebug(mem_map, "After allocations");
  /* skip (0) null ptr hole, (1) trampoline and text, and (2) rodata  */
  ASSERT_EQ(mem_map->vmentry[3]->page_num,
            (initial_addr - NACL_MAP_PAGESIZE) >> NACL_PAGESHIFT);
  ASSERT_EQ(mem_map->vmentry[3]->npages,
            NACL_PAGES_PER_MAP);

  ASSERT_EQ(mem_map->vmentry[4]->page_num,
            initial_addr >> NACL_PAGESHIFT);
  ASSERT_EQ(mem_map->vmentry[4]->npages,
            2 * NACL_PAGES_PER_MAP);

  ASSERT_EQ(mem_map->vmentry[5]->page_num,
            (initial_addr +  2 * NACL_MAP_PAGESIZE) >> NACL_PAGESHIFT);
  ASSERT_EQ(mem_map->vmentry[5]->npages,
            NACL_PAGES_PER_MAP);
  /* skip zero page, trampolines and executable */

  /*
   * Undo effects of previous mmaps
   */
  errcode = NaClSysMunmap(natp, (void *) (initial_addr - NACL_MAP_PAGESIZE),
                          NACL_MAP_PAGESIZE * 4);
  ASSERT_EQ(errcode, 0);
  ASSERT_EQ(mem_map->nvalid, 4);


  /* Allocate range */
  addr = NaClSysMmap(natp, 0, 9 * NACL_MAP_PAGESIZE,
                     NACL_ABI_PROT_READ | NACL_ABI_PROT_WRITE,
                     NACL_ABI_MAP_ANONYMOUS | NACL_ABI_MAP_PRIVATE, -1, 0);
  printf("addr=0x%"PRIxPTR"\n", addr);
  initial_addr = addr;

  /* Map into middle of previously allocated range */
  addr = NaClSysMmap(natp, (void *) (initial_addr + 2 * NACL_MAP_PAGESIZE),
                     3 * NACL_MAP_PAGESIZE,
                     NACL_ABI_PROT_READ | NACL_ABI_PROT_WRITE,
                     NACL_ABI_MAP_ANONYMOUS | NACL_ABI_MAP_PRIVATE
                     | NACL_ABI_MAP_FIXED,
                     -1, 0);
  printf("addr=0x%"PRIxPTR"\n", addr);
  ASSERT_EQ(addr, initial_addr + NACL_MAP_PAGESIZE * 2);

  NaClVmmapMakeSorted(mem_map);
  ASSERT_EQ(mem_map->nvalid, 7);

  ASSERT_EQ(mem_map->vmentry[3]->page_num,
            initial_addr >> NACL_PAGESHIFT);
  ASSERT_EQ(mem_map->vmentry[3]->npages,
            2 * NACL_PAGES_PER_MAP);

  ASSERT_EQ(mem_map->vmentry[4]->page_num,
            (initial_addr + 2 * NACL_MAP_PAGESIZE) >> NACL_PAGESHIFT);
  ASSERT_EQ(mem_map->vmentry[4]->npages,
            3 * NACL_PAGES_PER_MAP);

  ASSERT_EQ(mem_map->vmentry[5]->page_num,
            (initial_addr + 5 * NACL_MAP_PAGESIZE) >> NACL_PAGESHIFT);
  ASSERT_EQ(mem_map->vmentry[5]->npages,
            4 * NACL_PAGES_PER_MAP);

  /*
   * Undo effects of previous mmaps
   */
  errcode = NaClSysMunmap(natp, (void *) initial_addr, 9 * NACL_MAP_PAGESIZE);
  ASSERT_EQ(errcode, 0);
  ASSERT_EQ(mem_map->nvalid, 4);

  /*
   * Check use of hint.
   */
  addr = NaClSysMmap(natp, (void *) initial_addr,
                     NACL_MAP_PAGESIZE,
                     NACL_ABI_PROT_READ | NACL_ABI_PROT_WRITE,
                     NACL_ABI_MAP_ANONYMOUS | NACL_ABI_MAP_PRIVATE,
                     -1, 0);
  ASSERT_LE(addr, 0xffff0000u);
  printf("addr=0x%"PRIxPTR"\n", addr);
  ASSERT_LE_MSG(initial_addr, addr, "returned address not at or above hint");
  errcode = NaClSysMunmap(natp, (void *) addr, NACL_MAP_PAGESIZE);
  ASSERT_EQ(errcode, 0);

  /* Check handling of zero-sized mappings. */
  addr = NaClSysMmap(natp, 0, 0,
                     NACL_ABI_PROT_READ | NACL_ABI_PROT_WRITE,
                     NACL_ABI_MAP_ANONYMOUS | NACL_ABI_MAP_PRIVATE, -1, 0);
  ASSERT_EQ((int) addr, -NACL_ABI_EINVAL);

  errcode = NaClSysMunmap(natp, (void *) initial_addr, 0);
  ASSERT_EQ(errcode, -NACL_ABI_EINVAL);

  printf("PASS\n");
  return 0;
}
