/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_SEL_MAIN_CHROME_H_
#define NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_SEL_MAIN_CHROME_H_ 1

#include "native_client/src/include/nacl_base.h"
#include "native_client/src/shared/imc/nacl_imc_c.h"

EXTERN_C_BEGIN

struct NaClValidationCache;


/*
 * Descriptor number for initial_ipc_desc.  This is chosen so as not
 * to conflict with NACL_SERVICE_PORT_DESCRIPTOR,
 * NACL_SERVICE_ADDRESS_DESCRIPTOR and export_addr_to inside
 * NaClChromeMainStart().
 */
#define NACL_CHROME_INITIAL_IPC_DESC 6


struct NaClChromeMainArgs {
  /*
   * Handle for bootstrapping a NaCl IMC connection to the trusted
   * PPAPI plugin.  Required.
   */
  NaClHandle imc_bootstrap_handle;

  /*
   * File descriptor for the NaCl integrated runtime (IRT) library.
   * Note that this is a file descriptor even on Windows (where file
   * descriptors are emulated by the C runtime library).  Required.
   */
  int irt_fd;

  /*
   * Descriptor to provide to untrusted code as descriptor number
   * NACL_CHROME_INITIAL_IPC_DESC.  For use by the Chrome-IPC-based
   * PPAPI proxy.  Optional; may be NULL.
   */
  struct NaClDesc *initial_ipc_desc;

  /* Whether to enable untrusted hardware exception handling.  Boolean. */
  int enable_exception_handling;

  /* Whether to enable NaCl's built-in GDB RSP debug stub.  Boolean. */
  int enable_debug_stub;

  /*
   * Callback to use for creating shared memory objects.  Optional;
   * may be NULL.
   */
  NaClCreateMemoryObjectFunc create_memory_object_func;

  /* Cache for NaCl validation judgements.  Optional; may be NULL. */
  struct NaClValidationCache *validation_cache;

#if NACL_WINDOWS
  /*
   * Callback to use instead of DuplicateHandle() for copying a
   * Windows handle to another process.  Optional; may be NULL.
   */
  NaClBrokerDuplicateHandleFunc broker_duplicate_handle_func;

  /*
   * Callback to use for requesting that a debug exception handler be
   * attached to this process for handling hardware exceptions via the
   * Windows debug API.  The data in info/info_size must be passed to
   * NaClDebugExceptionHandlerRun().  Optional; may be NULL.
   */
  int (*attach_debug_exception_handler_func)(const void *info,
                                             size_t info_size);
#endif
};

/* Create a new args struct containing default values. */
struct NaClChromeMainArgs *NaClChromeMainArgsCreate(void);

/* Launch NaCl. */
void NaClChromeMainStart(struct NaClChromeMainArgs *args);


EXTERN_C_END

#endif
