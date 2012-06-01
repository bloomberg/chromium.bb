/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * This is the source used to create the auxv_phdr.nexe binaries in testdata.
 * See http://code.google.com/p/nativeclient/issues/detail?id=2823 about
 * how those should be replaced with a compiled test when capable toolchains
 * become available.
 */

#include <stdio.h>
#include <unistd.h>

#include "native_client/src/include/elf32.h"
#include "native_client/src/include/elf_auxv.h"
#include "native_client/src/untrusted/nacl/tls.h"
#include "native_client/src/untrusted/nacl/nacl_irt.h"
#include "native_client/src/untrusted/nacl/nacl_startup.h"


void __libc_init_array(void);
void __libc_fini_array(void);

static int check_auxv(Elf32_auxv_t *auxv) {
  Elf32_auxv_t *phdr = NULL;
  Elf32_auxv_t *phent = NULL;
  Elf32_auxv_t *phnum = NULL;
  Elf32_auxv_t *av;

  for (av = auxv; av->a_type != AT_NULL; ++av) {
    switch (av->a_type) {
      case AT_PHDR:
        phdr = av;
        break;
      case AT_PHENT:
        phent = av;
        break;
      case AT_PHNUM:
        phnum = av;
        break;
    }
  }

  if (phdr == NULL)
    printf("Did not find AT_PHDR in %d entries!\n", (int) (av - auxv));
  if (phent == NULL)
    printf("Did not find AT_PHENT in %d entries!\n", (int) (av - auxv));
  if (phnum == NULL)
    printf("Did not find AT_PHNUM in %d entries!\n", (int) (av - auxv));

  if (phdr != NULL && phent != NULL && phnum != NULL) {
    const Elf32_Phdr *const ph = (const void *) phdr->a_un.a_val;
    Elf32_Word i;

    printf("%u program headers of size %u at %#x\n",
           phnum->a_un.a_val, phent->a_un.a_val, phdr->a_un.a_val);

    for (i = 0; i < phnum->a_un.a_val; ++i) {
      printf("%-8u %#.8x %#.8x %#.8x %#.8x %#.8x %#x %#x\n",
             ph[i].p_type, ph[i].p_offset, ph[i].p_vaddr,
             ph[i].p_paddr, ph[i].p_filesz, ph[i].p_memsz,
             ph[i].p_flags, ph[i].p_align);
    }

    return 0;
  }

  return 1;
}

/*
 * This is the true entry point for untrusted code.
 * See nacl_startup.h for the layout at the argument pointer.
 */
void _start(uint32_t *info) {
  void (*fini)(void) = nacl_startup_fini(info);
  char **envp = nacl_startup_envp(info);
  Elf32_auxv_t *auxv = nacl_startup_auxv(info);

  environ = envp;

  __libnacl_irt_init(auxv);

  /*
   * If we were started by a dynamic linker, then it passed its finalizer
   * function here.  For static linking, this is always NULL.
   */
  if (fini != NULL)
    atexit(fini);

  atexit(&__libc_fini_array);

  __pthread_initialize();

  __libc_init_array();

  exit(check_auxv(auxv));

  /*NOTREACHED*/
  while (1) *(volatile int *) 0;  /* Crash.  */
}
