/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/untrusted/crt/crthelper.h"

ADD_FUN_PTR_TO_SEC(__CTOR_END__[1], ".ctors", FUN_PTR_END_MARKER);
ADD_FUN_PTR_TO_SEC(__DTOR_END__[1], ".dtors", FUN_PTR_END_MARKER);
/* NOTE: this is really a 32bit quantity, but in our ILP32 world this is ok */
ADD_FUN_PTR_TO_SEC(__FRAME_END__[1], ".eh_frame", FUN_PTR_END_MARKER);

static void ATTR_USED __do_global_ctors_aux (void) {
  FUN_PTR *fun;
  for (fun = __CTOR_END__ - 1; FUN_PTR_BEGIN_MARKER != *fun ; --fun) {
    (*fun)();
  }
}

ADD_FUN_PTR_TO_SEC(__do_global_ctors_aux_init_array_entry[1],
                   ".init_array", __do_global_ctors_aux);
