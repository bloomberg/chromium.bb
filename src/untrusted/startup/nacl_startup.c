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
extern void _fini();
extern void _init();
extern int main(int argc, char *argv[], char *envp[]);
extern void __srpc_init();
extern void __srpc_wait();
extern void exit(int result);
extern void __pthread_initialize();
extern void __pthread_shutdown();
extern void atexit(void (*funptr)());
extern void __av_wait();

void __nacl_startup(int argc, char *argv[], char *envp[]) {
  int result;
  /*
   * Install the fini section for use at exit.  The C++ static object
   * destructors are invoked from here.
   * Invoke __nacl_startup which ultimately calls main.
   */
  atexit(_fini);
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
   */
  _init();
  /*
   * Initialize the SRPC module before starting main.  There is a weak
   * definition in libnacl that can be overridden by libsrpc.
   */
  __srpc_init();
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
