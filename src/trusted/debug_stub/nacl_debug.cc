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

#include "native_client/src/trusted/gdb_rsp/session.h"
#include "native_client/src/trusted/gdb_rsp/target.h"
#include "native_client/src/trusted/port/platform.h"
#include "native_client/src/trusted/port/thread.h"

#include "native_client/src/include/nacl_string.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_exit.h"
#include "native_client/src/shared/platform/nacl_threads.h"
#include "native_client/src/trusted/debug_stub/debug_stub.h"
#include "native_client/src/trusted/debug_stub/nacl_debug.h"
#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"
#include "native_client/src/trusted/service_runtime/nacl_debug_init.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"

#if NACL_WINDOWS
# include "native_client/src/trusted/service_runtime/win/debug_exception_handler.h"
#endif

using port::IPlatform;
using port::IThread;
using port::ITransport;

using gdb_rsp::Session;
using gdb_rsp::Target;

#ifdef WIN32
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


static const struct NaClDebugCallbacks debug_callbacks = {
  NaClDebugThreadPrepDebugging,
  NaClDebugThreadStopDebugging,
  NaClDebugStop,
};

static Target *g_target = NULL;

void WINAPI NaClStubThread(void *ptr) {
  Target *targ = reinterpret_cast<Target*>(ptr);
  while (1) {
    ITransport* trans = NULL;
    Session* ses = NULL;

    try {
      // Wait for a connection.
      trans = ITransport::Accept("127.0.0.1:4014");
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

void NaClDebugThreadPrepDebugging(struct NaClAppThread *natp) throw() {
  UNREFERENCED_PARAMETER(natp);

  uint32_t id = IPlatform::GetCurrentThread();
  IThread* thread = IThread::Create(id, natp);
  g_target->SetMemoryBase(natp->nap->mem_start);
  g_target->TrackThread(thread);
}

void NaClDebugThreadStopDebugging(struct NaClAppThread *natp) throw() {
  UNREFERENCED_PARAMETER(natp);

  uint32_t id = IPlatform::GetCurrentThread();
  IThread* thread = IThread::Acquire(id);
  g_target->IgnoreThread(thread);
  IThread::Release(thread);  /* for Acquire */
  IThread::Release(thread);  /* for Create at NaClDebugThreadPrepDebugging */
}

void NaClDebugStop(int ErrCode) throw() {
  /*
   * We check if debugging is enabled since this check is the only
   * mechanism for allocating the state object.  We free the
   * resources but not the object itself.  Instead we mark it as
   * STOPPED to prevent it from getting recreated.
   */
  g_target->Exit(ErrCode);
  try {
    NaClDebugStubFini();
  } DBG_CATCH_ALL
}

/*
 * This function is implemented for the service runtime.  The service runtime
 * declares the function so it does not need to be declared in our header.
 */
int NaClDebugInit(struct NaClApp *nap) {
  nap->debug_stub_callbacks = &debug_callbacks;
  nap->enable_faulted_thread_queue = 1;
  NaClDebugStubInit();

  CHECK(g_target == NULL);
  g_target = new Target(nap);
  CHECK(g_target != NULL);
  g_target->Init();

  NaClThread *thread = new NaClThread;
  CHECK(thread != NULL);

  /* Add a temp breakpoint. */
  if (0 != nap->user_entry_pt) {
    g_target->AddTemporaryBreakpoint(nap->user_entry_pt + nap->mem_start);
  }
  g_target->AddTemporaryBreakpoint(nap->initial_entry_pt + nap->mem_start);

#if NACL_WINDOWS
  if (!NaClDebugExceptionHandlerEnsureAttached(nap)) {
    NaClLog(LOG_ERROR, "NaClDebugInit: "
            "Failed to attach debug exception handler\n");
    return 0;
  }
#endif

  NaClLog(LOG_WARNING, "nacl_debug(%d) : Debugging started.\n", __LINE__);
  CHECK(NaClThreadCtor(thread, NaClStubThread, g_target,
                       NACL_KERN_STACK_SIZE));

  return 1;
}
