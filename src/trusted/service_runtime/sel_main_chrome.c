/*
 * Copyright 2009 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/include/portability.h"

#if NACL_OSX
#include <crt_externs.h>
#endif

#ifdef _WIN64  /* TODO(gregoryd): remove this when win64 issues are fixed */
#define NACL_NO_INLINE
#endif

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/trusted/service_runtime/nacl_globals.h"
#include "native_client/src/trusted/service_runtime/env_cleanser.h"
#include "native_client/src/trusted/service_runtime/nacl_app.h"
#include "native_client/src/trusted/service_runtime/nacl_all_modules.h"
#include "native_client/src/trusted/service_runtime/nacl_debug.h"
#include "native_client/src/trusted/service_runtime/nacl_signal.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/service_runtime/sel_qualify.h"

static int const kSrpcFd = 5;

int verbosity = 0;


int NaClMainForChromium(int handle_count, const NaClHandle *handles,
                        int debug) {
  char *av[1];
  int ac = 1;
  const char **envp;
  struct NaClApp state;
  int export_addr_to = kSrpcFd; /* Used to be set by -X. */
  struct NaClApp *nap;
  NaClErrorCode errcode;
  int ret_code = 1;
  struct NaClEnvCleanser env_cleanser;

#if NACL_OSX
  /* Mac dynamic libraries cannot access the environ variable directly. */
  envp = (const char **) *_NSGetEnviron();
#else
  /* Overzealous code style check is overzealous. */
  /* @IGNORE_LINES_FOR_CODE_HYGIENE[1] */
  extern char **environ;
  envp = (const char **) environ;
#endif

  NaClAllModulesInit();

  /* to be passed to NaClMain, eventually... */
  av[0] = "NaClMain";

  if (!NaClAppCtor(&state)) {
    fprintf(stderr, "Error while constructing app state\n");
    goto done;
  }

  nap = &state;
  errcode = LOAD_OK;

  /* import IMC handle - used to be "-i" */
  CHECK(handle_count == 3);
  NaClAddImcHandle(nap, handles[0], export_addr_to);
  NaClAddImcHandle(nap, handles[1], 6); /* async_receive_desc */
  NaClAddImcHandle(nap, handles[2], 7); /* async_send_desc */

  /*
   * in order to report load error to the browser plugin through the
   * secure command channel, we do not immediate jump to cleanup code
   * on error.  rather, we continue processing (assuming earlier
   * errors do not make it inappropriate) until the secure command
   * channel is set up, and then bail out.
   */

  /*
   * Ensure this operating system platform is supported.
   */
  errcode = NaClRunSelQualificationTests();
  if (LOAD_OK != errcode) {
    nap->module_load_status = errcode;
    fprintf(stderr, "Error while loading in SelMain: %s\n",
            NaClErrorString(errcode));
  }

  /* Give debuggers a well known point at which xlate_base is known.  */
  NaClGdbHook(&state);

  /*
   * If export_addr_to is set to a non-negative integer, we create a
   * bound socket and socket address pair and bind the former to
   * descriptor 3 and the latter to descriptor 4.  The socket address
   * is written out to the export_addr_to descriptor.
   *
   * The service runtime also accepts a connection on the bound socket
   * and spawns a secure command channel thread to service it.
   *
   * If export_addr_to is -1, we only create the bound socket and
   * socket address pair, and we do not export to an IMC socket.  This
   * use case is typically only used in testing, where we only "dump"
   * the socket address to stdout or similar channel.
   */
  if (-2 < export_addr_to) {
    NaClCreateServiceSocket(nap);
    if (0 <= export_addr_to) {
      NaClSendServiceAddressTo(nap, export_addr_to);
      /*
       * NB: spawns a thread that uses the command channel.  we do
       * this after NaClAppLoadFile so that NaClApp object is more
       * fully populated.  Hereafter any changes to nap should be done
       * while holding locks.
       */
      NaClSecureCommandChannel(nap);

      NaClLog(4, "NaClSecureCommandChannel has spawned channel\n");
    }
  }
  NaClLog(4, "secure service = %"NACL_PRIxPTR"\n",
          (uintptr_t) nap->secure_service);

  if (NULL != nap->secure_service && LOAD_OK == errcode) {
    /*
     * wait for start_module RPC call on secure channel thread.
     */
    errcode = NaClWaitForStartModuleCommand(nap);
  }

  /*
   * error reporting done; can quit now if there was an error earlier.
   */
  if (LOAD_OK != errcode) {
    goto done;
  }

  /*
   * Enable debugging if requested.
   */
  if (debug) NaClDebugSetAllow(1);

  NaClEnvCleanserCtor(&env_cleanser);
  if (!NaClEnvCleanserInit(&env_cleanser, envp)) {
    NaClLog(LOG_FATAL, "Failed to initialise env cleanser\n");
  }

  /*
   * only nap->ehdrs.e_entry is usable, no symbol table is
   * available.
   */
  if (!NaClCreateMainThread(nap, ac, av,
                            NaClEnvCleanserEnvironment(&env_cleanser))) {
    fprintf(stderr, "creating main thread failed\n");
    goto done;
  }

  NaClEnvCleanserDtor(&env_cleanser);

  ret_code = NaClWaitForMainThreadToExit(nap);

  /*
   * exit_group or equiv kills any still running threads while module
   * addr space is still valid.  otherwise we'd have to kill threads
   * before we clean up the address space.
   */
  return ret_code;

 done:
  fflush(stdout);

  NaClAllModulesFini();

  return ret_code;
}
