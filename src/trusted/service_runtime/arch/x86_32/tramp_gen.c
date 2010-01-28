/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <stdio.h>
#include "native_client/src/include/portability.h"
#include "native_client/src/trusted/service_runtime/tramp.h"


int main() {
  unsigned char * idx = (unsigned char *) &NaCl_trampoline_seg_code;

  /* print out the offsets used for patching */
  printf("#define NACL_TRAMP_CSEG_PATCH 0x%02"PRIxPTR"\n",
         ((uintptr_t) &NaCl_tramp_cseg_patch -
          (uintptr_t) &NaCl_trampoline_seg_code));
  printf("#define NACL_TRAMP_DSEG_PATCH 0x%02"PRIxPTR"\n",
         ((uintptr_t) &NaCl_tramp_dseg_patch -
          (uintptr_t) &NaCl_trampoline_seg_code));
  printf("\n");

  printf("unsigned char kTrampolineCode[] = {\n");
  while (idx <= (unsigned char *) &NaCl_trampoline_seg_end) {
    printf(" 0x%02x,", *idx);
    idx++;
  }
  printf("\n};\n");

  return 0;
}
