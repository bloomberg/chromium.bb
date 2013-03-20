/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/service_runtime/sel_main_chrome.h"

#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/src/include/portability_sockets.h"

#if NACL_OSX
#include <crt_externs.h>
#endif

#include <stdio.h>
#include <string.h>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_exit.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_secure_random.h"
#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/trusted/desc/nacl_desc_io.h"
#include "native_client/src/trusted/fault_injection/fault_injection.h"
#include "native_client/src/trusted/gio/gio_nacl_desc.h"
#include "native_client/src/trusted/service_runtime/env_cleanser.h"
#include "native_client/src/trusted/service_runtime/include/sys/fcntl.h"
#include "native_client/src/trusted/service_runtime/nacl_all_modules.h"
#include "native_client/src/trusted/service_runtime/nacl_app.h"
#include "native_client/src/trusted/service_runtime/nacl_bootstrap_channel_error_reporter.h"
#include "native_client/src/trusted/service_runtime/nacl_error_log_hook.h"
#include "native_client/src/trusted/service_runtime/nacl_globals.h"
#include "native_client/src/trusted/service_runtime/nacl_debug_init.h"
#include "native_client/src/trusted/service_runtime/nacl_signal.h"
#include "native_client/src/trusted/service_runtime/osx/mach_exception_handler.h"
#include "native_client/src/trusted/service_runtime/sel_addrspace.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/service_runtime/sel_qualify.h"
#include "native_client/src/trusted/service_runtime/win/exception_patch/ntdll_patch.h"

struct NaClChromeMainArgs *NaClChromeMainArgsCreate(void) {
  struct NaClChromeMainArgs *args = malloc(sizeof(*args));
  if (args == NULL)
    return NULL;
  args->imc_bootstrap_handle = NACL_INVALID_HANDLE;
  args->irt_fd = -1;
  args->initial_ipc_desc = NULL;
  args->enable_exception_handling = 0;
  args->enable_debug_stub = 0;
#if NACL_LINUX || NACL_OSX
  args->debug_stub_server_bound_socket_fd = NACL_INVALID_SOCKET;
#endif
  args->create_memory_object_func = NULL;
  args->validation_cache = NULL;
#if NACL_WINDOWS
  args->broker_duplicate_handle_func = NULL;
  args->attach_debug_exception_handler_func = NULL;
#endif
#if NACL_LINUX || NACL_OSX
  args->urandom_fd = -1;
#endif
#if NACL_LINUX
  args->prereserved_sandbox_size = 0;
#endif

  /*
   * Initialize NaClLog so that Chromium can call
   * NaClDescMakeCustomDesc() between calling
   * NaClChromeMainArgsCreate() and NaClChromeMainStart().
   */
  NaClLogModuleInit();

  return args;
}

static void NaClLoadIrt(struct NaClApp *nap, int irt_fd) {
  int file_desc;
  struct GioPio gio_pio;
  struct Gio *gio_desc;
  NaClErrorCode errcode;

  if (irt_fd == -1) {
    NaClLog(LOG_FATAL, "NaClLoadIrt: Integrated runtime (IRT) not present.\n");
  }

  file_desc = DUP(irt_fd);
  if (file_desc < 0) {
    NaClLog(LOG_FATAL, "NaClLoadIrt: Failed to dup() file descriptor\n");
  }

  /*
   * The GioPio type is safe to use when this file descriptor is shared
   * with other processes, because it does not use the shared file position.
   */
  if (!GioPioCtor(&gio_pio, file_desc)) {
    NaClLog(LOG_FATAL, "NaClLoadIrt: Failed to create Gio wrapper\n");
  }
  gio_desc = (struct Gio *) &gio_pio;

  errcode = NaClAppLoadFileDynamically(nap, gio_desc);
  if (errcode != LOAD_OK) {
    NaClLog(LOG_FATAL,
            "NaClLoadIrt: Failed to load the integrated runtime (IRT): %s\n",
            NaClErrorString(errcode));
  }

  (*NACL_VTBL(Gio, gio_desc)->Close)(gio_desc);
  (*NACL_VTBL(Gio, gio_desc)->Dtor)(gio_desc);
}

void NaClChromeMainStart(struct NaClChromeMainArgs *args) {
  char *av[1];
  int ac = 1;
  const char **envp;
  struct NaClApp state;
  struct NaClApp *nap = &state;
  NaClErrorCode errcode = LOAD_INTERNAL;
  int ret_code = 1;
  struct NaClEnvCleanser env_cleanser;
  int skip_qualification;

#if NACL_OSX
  /* Mac dynamic libraries cannot access the environ variable directly. */
  envp = (const char **) *_NSGetEnviron();
#else
  /* Overzealous code style check is overzealous. */
  /* @IGNORE_LINES_FOR_CODE_HYGIENE[1] */
  extern char **environ;
  envp = (const char **) environ;
#endif

#if NACL_LINUX || NACL_OSX
  /* This needs to happen before NaClAllModulesInit(). */
  if (args->urandom_fd != -1)
    NaClSecureRngModuleSetUrandomFd(args->urandom_fd);
#endif

  /*
   * Clear state so that NaClBootstrapChannelErrorReporter will be
   * able to know if the bootstrap channel is available or not.
   */
  memset(&state, 0, sizeof state);
  NaClAllModulesInit();
  NaClBootstrapChannelErrorReporterInit();
  NaClErrorLogHookInit(NaClBootstrapChannelErrorReporter, &state);

  /* to be passed to NaClMain, eventually... */
  av[0] = "NaClMain";

  if (NACL_FI_ERROR_COND("AppCtor", !NaClAppCtor(&state))) {
    NaClLog(LOG_FATAL, "Error while constructing app state\n");
    goto done;
  }

  errcode = LOAD_OK;

#if NACL_LINUX
  g_prereserved_sandbox_size = args->prereserved_sandbox_size;
#endif

  if (args->create_memory_object_func != NULL)
    NaClSetCreateMemoryObjectFunc(args->create_memory_object_func);

  /* Inject the validation caching interface, if it exists. */
  nap->validation_cache = args->validation_cache;

#if NACL_WINDOWS
  if (args->broker_duplicate_handle_func != NULL)
    NaClSetBrokerDuplicateHandleFunc(args->broker_duplicate_handle_func);
#endif

  NaClAppInitialDescriptorHookup(nap);

  /*
   * NACL_SERVICE_PORT_DESCRIPTOR and NACL_SERVICE_ADDRESS_DESCRIPTOR
   * are 3 and 4.
   */

  if (args->initial_ipc_desc != NULL) {
    NaClSetDesc(nap, NACL_CHROME_INITIAL_IPC_DESC, args->initial_ipc_desc);
  }

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
  skip_qualification = getenv("NACL_DANGEROUS_SKIP_QUALIFICATION_TEST") != NULL;
  if (skip_qualification) {
    fprintf(stderr, "PLATFORM QUALIFICATION DISABLED - "
        "Native Client's sandbox will be unreliable!\n");
  } else {
    errcode = NACL_FI_VAL("pq", NaClErrorCode,
                          NaClRunSelQualificationTests());
    if (LOAD_OK != errcode) {
      nap->module_load_status = errcode;
      fprintf(stderr, "Error while loading in SelMain: %s\n",
              NaClErrorString(errcode));
    }
  }

  /*
   * Patch the Windows exception dispatcher to be safe in the case
   * of faults inside x86-64 sandboxed code.  The sandbox is not
   * secure on 64-bit Windows without this.
   */
#if (NACL_WINDOWS && NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && \
     NACL_BUILD_SUBARCH == 64)
  NaClPatchWindowsExceptionDispatcher();
#endif
  NaClSignalTestCrashOnStartup();

  nap->enable_exception_handling = args->enable_exception_handling;

  if (args->enable_exception_handling || args->enable_debug_stub) {
#if NACL_LINUX
    /* NaCl's signal handler is always enabled on Linux. */
#elif NACL_OSX
    if (!NaClInterceptMachExceptions()) {
      NaClLog(LOG_FATAL, "NaClChromeMainStart: "
              "Failed to set up Mach exception handler\n");
    }
#elif NACL_WINDOWS
    nap->attach_debug_exception_handler_func =
        args->attach_debug_exception_handler_func;
#else
# error Unknown host OS
#endif
  }
#if NACL_LINUX
  NaClSignalHandlerInit();
#endif

  /* Give debuggers a well known point at which xlate_base is known.  */
  NaClGdbHook(&state);

  NaClCreateServiceSocket(nap);
  /*
   * LOG_FATAL errors that occur before NaClSetUpBootstrapChannel will
   * not be reported via the crash log mechanism (for Chromium
   * embedding of NaCl, shown in the JavaScript console).
   *
   * Some errors, such as due to NaClRunSelQualificationTests, do not
   * trigger a LOG_FATAL but instead set module_load_status to be sent
   * in the start_module RPC reply.  Log messages associated with such
   * errors would be seen, since NaClSetUpBootstrapChannel will get
   * called.
   */
  NaClSetUpBootstrapChannel(nap, args->imc_bootstrap_handle);

  NACL_FI_FATAL("BeforeSecureCommandChannel");
  /*
   * NB: Spawns a thread that uses the command channel.  We do this
   * after NaClAppLoadFile so that the NaClApp object is more fully
   * populated.  Hereafter any changes to nap should be done while
   * holding locks.
   */
  NaClSecureCommandChannel(nap);

  NaClLog(4, "NaClSecureCommandChannel has spawned channel\n");

  NaClLog(4, "secure service = %"NACL_PRIxPTR"\n",
          (uintptr_t) nap->secure_service);
  NACL_FI_FATAL("BeforeWaitForStartModule");

  if (NULL != nap->secure_service) {
    NaClErrorCode start_result;
    /*
     * wait for start_module RPC call on secure channel thread.
     */
    start_result = NaClWaitForStartModuleCommand(nap);
    if (LOAD_OK == errcode) {
      errcode = start_result;
    }
  }

  NACL_FI_FATAL("BeforeLoadIrt");

  /*
   * error reporting done; can quit now if there was an error earlier.
   */
  if (LOAD_OK != errcode) {
    goto done;
  }

  /*
   * Load the integrated runtime (IRT) library.
   */
  if (args->irt_fd != -1 && !nap->irt_loaded) {
    NaClLoadIrt(nap, args->irt_fd);
    nap->irt_loaded = 1;
  }

  NACL_FI_FATAL("BeforeEnvCleanserCtor");

  NaClEnvCleanserCtor(&env_cleanser, 1);
  if (!NaClEnvCleanserInit(&env_cleanser, envp, NULL)) {
    NaClLog(LOG_FATAL, "Failed to initialise env cleanser\n");
  }

  if (NACL_FI_ERROR_COND("LaunchServiceThreads",
                         !NaClAppLaunchServiceThreads(nap))) {
    NaClLog(LOG_FATAL, "Launch service threads failed\n");
  }

  if (args->enable_debug_stub) {
#if NACL_LINUX || NACL_OSX
    if (args->debug_stub_server_bound_socket_fd != NACL_INVALID_SOCKET) {
      NaClDebugSetBoundSocket(args->debug_stub_server_bound_socket_fd);
    }
#endif
    if (!NaClDebugInit(nap)) {
      goto done;
    }
  }

  free(args);
  args = NULL;

  if (NACL_FI_ERROR_COND(
          "CreateMainThread",
          !NaClCreateMainThread(nap, ac, av,
                                NaClEnvCleanserEnvironment(&env_cleanser)))) {
    NaClLog(LOG_FATAL, "creating main thread failed\n");
  }
  NACL_FI_FATAL("BeforeEnvCleanserDtor");

  NaClEnvCleanserDtor(&env_cleanser);

  ret_code = NaClWaitForMainThreadToExit(nap);

  if (NACL_ABI_WIFEXITED(nap->exit_status)) {
    /*
     * Under Chrome, a call to _exit() often indicates that something
     * has gone awry, so we report it here to aid debugging.
     *
     * This conditional does not run if the NaCl process was
     * terminated forcibly, which is the normal case under Chrome.
     * This forcible exit is triggered by the renderer closing the
     * trusted SRPC channel, which we record as NACL_ABI_SIGKILL
     * internally.
     */
    NaClLog(LOG_INFO, "NaCl untrusted code called _exit(0x%x)\n", ret_code);
  }

  /*
   * exit_group or equiv kills any still running threads while module
   * addr space is still valid.  otherwise we'd have to kill threads
   * before we clean up the address space.
   */
  NaClExit(ret_code);

 done:
  fflush(stdout);

  /*
   * If there is a secure command channel, we sent an RPC reply with
   * the reason that the nexe was rejected.  If we exit now, that
   * reply may still be in-flight and the various channel closure (esp
   * reverse channel) may be detected first.  This would result in a
   * crash being reported, rather than the error in the RPC reply.
   * Instead, we wait for the hard-shutdown on the command channel.
   */
  if (LOAD_OK != errcode) {
    NaClBlockIfCommandChannelExists(nap);
  }

  NaClAllModulesFini();

  NaClExit(ret_code);
}
