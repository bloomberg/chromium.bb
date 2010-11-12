/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/untrusted/crt/crthelper.h"


/* NOTE: currently disable as it would pull in parts
   of libgcc_eh.a which depend on free/malloc
*/
#define HAVE_EH
#ifdef HAVE_EH
/* HACK HACK HACK */
/* The real structure is defined in llvm-gcc-4.2/gcc/unwind-dw2-fde.h
   this is something that is at least twice as big.
*/
struct object {
  void *p[16];
};

/* NOTE: __register_frameXXX() are provided by libgcc_eh.a, code can be found
 * here: llvm-gcc-4.2/gcc/unwind-dw2-fde.c
 * traditionally gcc uses weak linkage magic to making linking this library
 * in optional.
 * To simplify our TC we will always link this in.
 */

/* @IGNORE_LINES_FOR_CODE_HYGIENE[2] */
extern void __deregister_frame_info (const void *);
extern void __register_frame_info(void *begin, struct object *ob);
#endif

/* __dso_handle is zero on the main executable. */
void *__dso_handle = 0;

/* add start markers to the beginnings of .ctors, etc. */
/* similar stop markers are added in crtend.c */
ADD_FUN_PTR_TO_SEC(__CTOR_LIST__[1], ".ctors", FUN_PTR_BEGIN_MARKER);
ADD_FUN_PTR_TO_SEC(__DTOR_LIST__[1], ".dtors", FUN_PTR_BEGIN_MARKER);
ADD_FUN_PTR_TO_SEC(__EH_FRAME_BEGIN__[0], ".eh_frame",);


static void ATTR_USED __do_global_dtors_aux (void) {
  FUN_PTR *fun;
  for (fun = __DTOR_LIST__ + 1; FUN_PTR_END_MARKER != *fun; ++fun) {
    (*fun)();
  }

#ifdef HAVE_EH
  __deregister_frame_info (__EH_FRAME_BEGIN__);
#endif
}

ADD_FUN_PTR_TO_SEC(__do_global_dtors_aux_fini_array_entry[1],
                   ".fini_array", __do_global_dtors_aux);


#ifdef HAVE_EH
static void ATTR_USED frame_dummy (void) {
  static struct object object;
  __register_frame_info (__EH_FRAME_BEGIN__, &object);
}

/* schedule initialization of the exception handling system for c++
 * before main() is called.
 */
ADD_FUN_PTR_TO_SEC(__FRAME_DUMMY__[1], ".init_array", frame_dummy);
#endif
