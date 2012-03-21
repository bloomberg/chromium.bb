/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Windows-specific routines for verifying that Data Execution Prevention is
 * functional.
 */

#include "native_client/src/include/portability.h"

#include <setjmp.h>
#include <stdlib.h>
#include <stdio.h>

#include "native_client/src/trusted/platform_qualify/nacl_dep_qualify.h"
#include "native_client/src/trusted/service_runtime/nacl_signal.h"

static int g_SigFound;

static enum NaClSignalResult signal_catch(int signal, void *ctx) {
  UNREFERENCED_PARAMETER(ctx);

  if (11 == signal) {
    g_SigFound = signal;
    return NACL_SIGNAL_SKIP;
  }
  return NACL_SIGNAL_SEARCH; /* some other signal we weren't expecting */
}

static int setup_signals() {
  return NaClSignalHandlerAdd(signal_catch);
}

static void restore_signals(int handlerId) {
  if (0 == NaClSignalHandlerRemove(handlerId)) {
    /* What to do if the remove failed? */
    fprintf(stderr, "Failed to unload handler.\n");
  }
}

int NaClAttemptToExecuteDataAtAddr(char *thunk_buffer, size_t size) {
  int result;
  int handlerId;
  nacl_void_thunk thunk = NaClGenerateThunk(thunk_buffer, size);

  handlerId = setup_signals();
  g_SigFound = 0;
  __try {
    thunk();
  } __except (EXCEPTION_EXECUTE_HANDLER) {
  }

  /* Should be a segfault */
  if (11 == g_SigFound) {
    result = 1;
  } else {
    result = 0;
  }

  restore_signals(handlerId);
  return result;
}

/*
 * Returns 1 if Data Execution Prevention is present and working.
 */
int NaClAttemptToExecuteData() {
  int result;
  char *thunk_buffer = malloc(64);
  if (NULL == thunk_buffer) {
    return 0;
  }
  result = NaClAttemptToExecuteDataAtAddr(thunk_buffer, 64);
  free(thunk_buffer);
  return result;
}
