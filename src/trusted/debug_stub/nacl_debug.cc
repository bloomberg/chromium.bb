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

void WINAPI NaClStubThread(void *thread_arg) {
  SocketBinding *socket_binding = static_cast<SocketBinding*>(thread_arg);
  if (socket_binding == NULL) {
    const char *addr = "127.0.0.1:4014";
    socket_binding = SocketBinding::Bind(addr);
    if (socket_binding == NULL) {
      NaClLog(LOG_ERROR, "NaClStubThread: Failed to bind TCP port '%s'\n",
              addr);
      return;
    }
  }

  while (1) {
    // Wait for a connection.
    nacl::scoped_ptr<ITransport> trans(socket_binding->AcceptConnection());
    if (NULL == trans.get()) continue;

    // Create a new session for this connection
    Session ses(trans.get());
    ses.SetFlags(Session::DEBUG_MASK);

    // Run this session for as long as it lasts
    g_target->Run(&ses);
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
  NaClDebugStubFini();
}

static const struct NaClDebugCallbacks debug_callbacks = {
  ThreadCreateHook,
  ThreadExitHook,
  ProcessExitHook,
};

/*
 * This function is implemented for the service runtime.  The service runtime
 * declares the function so it does not need to be declared in our header.
 */
int NaClDebugInit(struct NaClApp *nap,
                  NaClSocketHandle debug_stub_server_bound_socket_fd) {
  if (!NaClFaultedThreadQueueEnable(nap)) {
    NaClLog(LOG_ERROR, "NaClDebugInit: Failed to initialize fault handling\n");
    return 0;
  }
  nap->debug_stub_callbacks = &debug_callbacks;
  NaClDebugStubInit();

  CHECK(g_target == NULL);
  g_target = new Target(nap);
  CHECK(g_target != NULL);
  g_target->Init();

  NaClThread *thread = new NaClThread;
  CHECK(thread != NULL);

  NaClLog(LOG_WARNING, "nacl_debug(%d) : Debugging started.\n", __LINE__);
  SocketBinding *socket_binding = NULL;
  if (debug_stub_server_bound_socket_fd != NACL_INVALID_SOCKET) {
    socket_binding = new SocketBinding(debug_stub_server_bound_socket_fd);
  }
  CHECK(NaClThreadCtor(thread, NaClStubThread, socket_binding,
                       NACL_KERN_STACK_SIZE));

  return 1;
}
