/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#include <vector>
#include <map>

/*
 * NaCl Functions for intereacting with debuggers
 */

#include "native_client/src/include/nacl_string.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_exit.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_threads.h"
#include "native_client/src/trusted/debug_stub/debug_stub.h"
#include "native_client/src/trusted/debug_stub/platform.h"
#include "native_client/src/trusted/debug_stub/session.h"
#include "native_client/src/trusted/debug_stub/target.h"
#include "native_client/src/trusted/debug_stub/thread.h"
#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"
#include "native_client/src/trusted/service_runtime/nacl_debug_init.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/service_runtime/thread_suspension.h"

using port::IPlatform;
using port::IThread;
using port::ITransport;
using port::SocketBinding;

using gdb_rsp::Session;
using gdb_rsp::Target;

#if NACL_WINDOWS
/* Disable warning for unwind disabled when exceptions used */
#pragma warning(disable:4530)
#endif

/*
 * These macro wraps all debugging stub calls to prevent C++ code called
 * by the debugging stub to throw and exception past the C API.  We use
 * this technique to allow the use of STL templates.   We catch bad_alloc
 * separately purely to provide information for debugging purposes.
 */
#define DBG_CATCH_ALL                                                       \
  catch(std::bad_alloc) {                                                  \
    NaClLog(LOG_FATAL, "nacl_debug(%d) : Failed to allocate.\n", __LINE__); \
    NaClExit(-1);                                                          \
  }                                                                         \
  catch(std::exception e) {                                                \
    NaClLog(LOG_FATAL, "nacl_debug(%d) : Caught exception: %s.\n",          \
            __LINE__ , e.what());                                           \
    NaClExit(-1);                                                          \
  }                                                                         \
  catch(...) {                                                             \
    NaClLog(LOG_FATAL, "nacl_debug(%d) : Unexpected exception.\n", __LINE__);\
    NaClExit(-1);                                                           \
  }


static Target *g_target = NULL;
static SocketBinding *g_socket_binding = NULL;

void WINAPI NaClStubThread(void *ptr) {
  Target *targ = reinterpret_cast<Target*>(ptr);
  while (1) {
    ITransport* trans = NULL;
    Session* ses = NULL;

    try {
      // Wait for a connection.
      trans = g_socket_binding->AcceptConnection();
      if (NULL == trans) continue;

      // Create a new session for this connection
      ses = new Session();
      ses->Init(trans);
      ses->SetFlags(Session::DEBUG_MASK);

      // Run this session for as long as it lasts
      targ->Run(ses);
    }
    catch(...) {
      delete ses;
      ITransport::Free(trans);
    }
  }
}

static void ThreadCreateHook(struct NaClAppThread *natp) throw() {
  g_target->TrackThread(natp);
}

static void ThreadExitHook(struct NaClAppThread *natp) throw() {
  g_target->IgnoreThread(natp);
}

static void ProcessExitHook(int ErrCode) throw() {
  g_target->Exit(ErrCode);
  try {
    NaClDebugStubFini();
  } DBG_CATCH_ALL
}

static const struct NaClDebugCallbacks debug_callbacks = {
  ThreadCreateHook,
  ThreadExitHook,
  ProcessExitHook,
};

int NaClDebugBindSocket() {
  if (g_socket_binding == NULL) {
    NaClDebugStubInit();
    const char *addr = "127.0.0.1:4014";
    g_socket_binding = SocketBinding::Bind(addr);
    if (g_socket_binding == NULL) {
      NaClLog(LOG_ERROR, "NaClDebugBindSocket: Failed to bind TCP port '%s'\n",
              addr);
      return 0;
    }
  }
  return 1;
}

/*
 * This function is implemented for the service runtime.  The service runtime
 * declares the function so it does not need to be declared in our header.
 */
int NaClDebugInit(struct NaClApp *nap) {
  if (!NaClDebugBindSocket()) {
    return 0;
  }
  if (!NaClFaultedThreadQueueEnable(nap)) {
    NaClLog(LOG_ERROR, "NaClDebugInit: Failed to initialize fault handling\n");
    return 0;
  }
  nap->debug_stub_callbacks = &debug_callbacks;

  CHECK(g_target == NULL);
  g_target = new Target(nap);
  CHECK(g_target != NULL);
  g_target->Init();

  NaClThread *thread = new NaClThread;
  CHECK(thread != NULL);

  NaClLog(LOG_WARNING, "nacl_debug(%d) : Debugging started.\n", __LINE__);
  CHECK(NaClThreadCtor(thread, NaClStubThread, g_target,
                       NACL_KERN_STACK_SIZE));

  return 1;
}
