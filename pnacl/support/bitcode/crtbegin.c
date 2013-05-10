/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * This is the bitcode crtbegin (crtbegin.bc)
 *
 * It provides a hook for running global destructors
 * when a C++ module is unloaded (by dl_close).
 * It is closely coupled with libstdc++.so
 */

/*
 * __dso_handle uniquely identifies each DSO.
 *
 * The C++ global destructor list (__cxa_atexit) is keyed by
 * __dso_handle. When a DSO is unloaded, only its global destructors
 * are executed.
 *
 * For more information, see:
 * http://gcc.gnu.org/ml/gcc-patches/1999-12n/msg00664.html
 */
#ifndef SHARED
 /* __dso_handle is zero on the main executable. */
 void *__dso_handle = 0;
#else
/* This value is used to differentiate between different DSO's */
void *__dso_handle = &__dso_handle;
#endif

#ifdef SHARED
extern void __cxa_finalize(void*) __attribute__ ((weak));
#endif

 /*
  * __attribute__((destructor)) places a call to the function in the
  * .fini_array section in PNaCl.  The function pointers in .fini_array
  * at exit.
  */
void __attribute__((destructor)) __do_global_dtors_aux(void) {
#ifdef SHARED
  /* TODO(pdox): Determine whether this call to __cxa_finalize
   * is actually needed or used. */
  if (__cxa_finalize)
    __cxa_finalize (__dso_handle);
#endif
 }
