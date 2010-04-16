/*
 * Copyright 2010 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Windows-specific routines for verifying that Data Execution Prevention is
 * functional.
 */

#include "native_client/src/include/portability.h"

#include <setjmp.h>
#include <stdlib.h>

#include "native_client/src/trusted/platform_qualify/nacl_dep_qualify.h"

/*
 * Returns 1 if Data Execution Prevention is present and working.
 */
int NaClAttemptToExecuteData() {
  int result;
  char *thunk_buffer = malloc(64);
  nacl_void_thunk thunk = NaClGenerateThunk(thunk_buffer, 64);

  __try {
    thunk();
    result = 0;
  } __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ?
              EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
    result = 1;
  }

  return result;
}
