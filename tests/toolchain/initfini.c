/*
 * Copyright 2010 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <stdio.h>


typedef void (*FUN_PTR) (void);

#define ATTR_SEC(sec)\
  __attribute__ ((__used__, section(sec), aligned(sizeof(FUN_PTR))))

#define ADD_FUN_PTR_TO_SEC(name, sec, ptr) \
  static FUN_PTR name ATTR_SEC(sec)  = { ptr }



#define MAKE_FUN(name) void name() { printf(#name "\n");}


#if 0
/* .ctors and .dtors are not supported in PNaCl. */
MAKE_FUN(fun_ctor)
ADD_FUN_PTR_TO_SEC(array_ctor[1], ".ctors", fun_ctor);

MAKE_FUN(fun_dtor)
ADD_FUN_PTR_TO_SEC(array_dtor[1], ".dtors", fun_dtor);
#endif  /* 0 */

MAKE_FUN(fun_preinit)
ADD_FUN_PTR_TO_SEC(array_preinit[1], ".preinit_array", fun_preinit);

MAKE_FUN(fun_init)
ADD_FUN_PTR_TO_SEC(array_init[1], ".init_array", fun_init);

MAKE_FUN(fun_fini)
ADD_FUN_PTR_TO_SEC(array_fini[1], ".fini_array", fun_fini);

/* NOTE: there is NO .prefini_array */

int main() {
  return 0;
}
