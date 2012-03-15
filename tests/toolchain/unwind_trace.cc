/*
 * Copyright (c) 2012 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* NOTE: because of fun pointer casting we need to disable -pedantic. */

#include <stdio.h>
#include <stdlib.h>
#include <unwind.h>
#include "native_client/tests/toolchain/utils.h"


int main(int argc, char* argv[]);
/* prevent inlining */
void recurse(int n)  __attribute__((noinline));
void DumpContext(struct _Unwind_Context* context)  __attribute__((noinline));
_Unwind_Reason_Code TraceCallback(struct _Unwind_Context* context, void* dummy)
  __attribute__((noinline));

void DumpContext(struct _Unwind_Context* context) {
  ASSERT(context != 0, "null context\n");

  printf("@@ ----------------------------------------------\n");
  printf("@@ CONTEXT: %p\n", (void*) context);

#if 0
/* for debugging only - the details of the individual regs are not exposed */
#define DWARF_FRAME_REGISTERS 4
  for (int i = 0; i < DWARF_FRAME_REGISTERS + 1; ++i) {
    printf("@@reg %d: %p\n", i, (void*) _Unwind_GetGR(context, i));
  }
#endif
  printf("@@ip %p\n", (void*)  _Unwind_GetIP(context));
  printf("@@region %p\n", (void*) _Unwind_GetRegionStart(context));
  printf("@@lsda %p\n", _Unwind_GetLanguageSpecificData(context));
  printf("@@cfa %p\n", (void*) _Unwind_GetCFA(context));
}

int count_other = 0;
int count_main = 0;
int count_recurse = 0;

_Unwind_Reason_Code TraceCallback(struct _Unwind_Context* context,
                                  void* dummy) {
  DumpContext(context);

  void* region_start = (void*) _Unwind_GetRegionStart(context);
  if (region_start == FUNPTR2PTR(recurse)) {
    ASSERT(count_main == 0, "unexpected recurse() frame after main");
    count_recurse += 1;
  } else if (region_start == FUNPTR2PTR(main)) {
    /* NOTE: no idea why main incurs up to two frames */
    ASSERT(count_main < 2, "unexpected main() frame after main");
    ASSERT(count_recurse != 0, "unexpected main() frame before recurse");
    ASSERT(count_other == 0, "unexpected main() frame after other");
    count_main += 1;
  } else {
    ASSERT(count_main != 0, " unexpected other frame");
    count_other += 1;
  }
  /* keep going */
  return _URC_NO_REASON;
}


void recurse(int n) {
  printf("recurse -> %d\n", n);
  if (n == 0) {
    if (_Unwind_Backtrace(TraceCallback, 0)) {
      printf("backtrace failed\n");
    }
    return;
  }
  recurse(n - 1);
  /* NOTE: this print statement also prevents this function
   * from tail recursing into itself.
   * On gcc this behavior can also be controlled using
   *   -foptimize-sibling-calls
   */
  printf("recurse <- %d\n", n);
}


int ROUNDS = 10;
int main(int argc, char* argv[]) {
  /* NOTE: confuse optimizer, argc is never 5555 */
  if (argc != 5555) {
    argc = ROUNDS;
  }
  printf("main %p recurse %p\n", FUNPTR2PTR(main), FUNPTR2PTR(recurse));

  recurse(argc);

  printf("counts  recurse:%d main:%d other:%d\n",
         count_recurse,
         count_main,
         count_other);
  ASSERT(count_main >= 1, "unexpected count_main");
  ASSERT(count_recurse == ROUNDS + 1, "unexpected count_recurse");
  return 55;
}
