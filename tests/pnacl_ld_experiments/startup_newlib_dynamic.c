/* Copyright 2012 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <unistd.h>

#include "native_client/src/include/elf32.h"
#include "native_client/src/untrusted/nacl/nacl_irt.h"
#include "native_client/src/untrusted/nacl/nacl_startup.h"

extern int main(int argc, char **argv, char **envp);

/*
 * This is the true entry point for untrusted code.
 * See nacl_startup.h for the layout at the argument pointer.
 */
 void _start(uint32_t *info) {
   void (*fini)(void) = nacl_startup_fini(info);
   int argc = nacl_startup_argc(info);
   char **argv = nacl_startup_argv(info);
   char **envp = nacl_startup_envp(info);
   Elf32_auxv_t *auxv = nacl_startup_auxv(info);

   /* environ = envp; */

   __libnacl_irt_init(auxv);

   /*
    * If we were started by a dynamic linker, then it passed its finalizer
    * function here.  For static linking, this is always NULL.
    */

   /* if (fini != NULL) atexit(fini); */

   /* atexit(&__libc_fini_array); */

   /* __pthread_initialize(); */

   /* __libc_init_array(); */

   exit(main(argc, argv, envp));

   /*NOTREACHED*/
   while (1) *(volatile int *) 0;  /* Crash.  */
 }
