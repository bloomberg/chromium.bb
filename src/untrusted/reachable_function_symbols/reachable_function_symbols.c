 /*
  * Copyright 2010 The Native Client Authors. All rights reserved.
  * Use of this source code is governed by a BSD-style license that can
  * be found in the LICENSE file.
  */

/*
 * The point of this code is to force a number of functions to be reachable,
 * so that the bitcode optimzer/linker does (llvm-ld) not throw them out.
 * These functions are called from startup code which is not visible to
 * the bitcode optimizer/linker.
 */

/* we ignore function signatures here */
/* @IGNORE_LINES_FOR_CODE_HYGIENE[8] */
extern void atexit();
extern void __pthread_initialize();
extern void __pthread_shutdown();
extern void __srpc_init();
extern void __av_wait();
extern void __srpc_wait();
extern void exit();
extern void raise();


typedef void (*Function)();

/*
 * NOTE: this must be globally visible so that the startup code can call them,
 *        so that llvm cannot make any assumptions
 */

 Function __KEEP_THESE_FUNCTIONS_ALIVE[] = {
  &atexit,
  &exit,
  &raise,
  &__av_wait,
  &__pthread_initialize,
  &__pthread_shutdown,
  &__srpc_init,
  &__srpc_wait,
};
