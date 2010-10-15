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
extern void __srpc_init();
extern void __srpc_wait();
extern void exit(int result);
extern void __pthread_initialize();
extern void __pthread_shutdown();
extern void atexit(void (*funptr)());
extern void __av_wait();

/*
 * We have to force the symbols below to be linked in as they
 * are referenced in libgcc_eh.a
 * c.f. http://code.google.com/p/nativeclient/issues/detail?id=1044
 */
void HackToForceSymbolsToBeLinkedIn(char *s) {
  /* @IGNORE_LINES_FOR_CODE_HYGIENE[4] */
  extern void abort();
  extern int strlen(const char* cp);
  extern void *malloc(int);
  extern void free(void*);

  char* d = malloc(10);
  if (d == 0) {
    malloc(strlen(s));
    abort();
  }
  free(d);
}

/*
 *  __nacl_startup is called from crt1XXX, it ultimately calls main().
 */
void __nacl_startup(int argc, char *argv[], char *envp[]) {
  int result;
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
  /*
   * Initialize the SRPC module before starting main.  There is a weak
   * definition in libnacl that can be overridden by libsrpc.
   */
  __srpc_init();

  HackToForceSymbolsToBeLinkedIn(argv[0]);

  /*
   * Wait for libav startup to connect to the browser.  There is a weak
   * definition in libnacl that can be overridden by libav.
   */
  __av_wait();

  result = main(argc, argv, envp);
  /*
   * Wait for srpc shutdown.  There is a weak definition in libnacl
   * that can be overridden by libsrpc.
   */
  __srpc_wait();
  /*
   * exit will also call atexit()
   */
  exit(result);
}
