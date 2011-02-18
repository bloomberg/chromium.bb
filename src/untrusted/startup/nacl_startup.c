/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


/*
 * Native Client startup wrapper
 * (NOTE: this code used to live in the various platform specific
 * assembler files.)
 */

/* avoid including header files in order to make this independent of anything */
/* @IGNORE_LINES_FOR_CODE_HYGIENE[10] */
extern void __libc_init_array();
extern void __libc_fini_array();
extern int main(int argc, char *argv[], char *envp[]);
extern void exit(int result);
extern void __pthread_initialize();
extern void __pthread_shutdown();
extern void atexit(void (*funptr)());

/*
 * We have to force the symbols below to be linked in as they
 * are referenced in libgcc_eh.a
 * c.f. http://code.google.com/p/nativeclient/issues/detail?id=1044
 */
/* @IGNORE_LINES_FOR_CODE_HYGIENE[4] */
extern void abort();
extern void* malloc(int);
extern void free(void*);
extern void* memcpy(void*, const void *, int);

typedef void  (*FUN_PTR)();
volatile FUN_PTR HACK_TO_KEEP_SYMBOLS_ALIVE[] = {
  (FUN_PTR) abort,
  (FUN_PTR) malloc,
  (FUN_PTR) free,
  (FUN_PTR) memcpy };

extern char** environ;

/*
 *  __nacl_startup is called from crt1XXX, it ultimately calls main().
 */
void __nacl_startup(int argc, char *argv[], char *envp[]) {
  int result;
  /*
   * Remember envp for use by getenv, etc.
   */
  environ = envp;
  /*
   * Install the fini section for use at exit.  The C++ static object
   * destructors are invoked from here.
   */
  atexit(__libc_fini_array);
  /*
   * Initialize the pthreads library.  We need to do at least a minimal
   * amount of initialization (e.g., set up gs) to allow thread local
   * storage references to work.  The default binding of the symbol
   * is weak, replaced by the real pthread library initialization when
   * present.
   */
  __pthread_initialize();
  /*
   * Install the pthread_shutdown call to be called at exit.
   */
  atexit(__pthread_shutdown);
  /*
   * Execute the init section before starting main.  The C++ static
   * object constructors are invoked from here.
   * The code can be found in:
   * newlib/libc/misc/init.c
   * It calls (in this order):
   * * all function pointers in[__preinit_array_start,  __preinit_array_end[
   *   ( the .preinit_array section)
   * * _init (the .init section beginning)
   * * all function pointers in [__init_array_start,  __init_array_end[
   *   ( the .init_array section)
   *
   * NOTE: there are three version of registering code to be run before main
   *       * emit code into .init
   *       * add a pointer to .init_array/.preinit_array section
   *       * add a pointer to .ctors section
   */
  __libc_init_array();
  result = main(argc, argv, envp);
  /*
   * exit will also call atexit()
   */
  exit(result);
}
