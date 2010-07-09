/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#include <vector>

/*
 * NaCl Functions for intereacting with debuggers
 */

#include "native_client/src/include/nacl_string.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/service_runtime/nacl_debug.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"

#ifdef WIN32
// Disable warning for unwind disabled when exceptions used
#pragma warning(disable:4530)
#endif

/*
 * These macro wraps all debugging stub calls to prevent C++ code called
 * by the debugging stub to throw and exception past the C API.  We use
 * this technique to allow the use of STL templates.   We catch bad_alloc
 * seperately purely to provide information for debugging purposes.
 */
#define DBG_CATCH_ALL                                                       \
  catch (std::bad_alloc) {                                                  \
    NaClLog(LOG_FATAL, "nacl_debug(%d) : Failed to allocate.\n", __LINE__); \
    exit(-1);                                                               \
  }                                                                         \
  catch (std::exception e) {                                                \
    NaClLog(LOG_FATAL, "nacl_debug(%d) : Caught exception: %s.\n",          \
            __LINE__ , e.what());                                           \
    exit(-1);                                                               \
  }                                                                         \
  catch (...) {                                                             \
    NaClLog(LOG_FATAL, "nacl_debug(%d) : Unexpected exception.\n", __LINE__);\
    exit(-1);                                                               \
  }


enum NaClDebugStatus {
  NDS_DISABLED = 0,
  NDS_ENABLED = 1,
  NDS_STOPPED = 2
};

uint32_t NaClDebugAllowed = 0;

struct NaClDebugState {
  NaClDebugState() : errCode_(0) {
#ifdef _DEBUG
    /*
     * When compiling DEBUG we allow an environment variable to enable
     * debugging, otherwise debugging could be allowed on a release
     * build by modifying NaClDebugAllowed.
     */
    if (NULL != getenv("NACL_DEBUG_ENABLE")) NaClDebugAllowed = 1;
#endif
    status_ = NaClDebugAllowed ? NDS_ENABLED : NDS_DISABLED;
  }

  NaClDebugStatus status_;
  nacl::string path_;
  std::vector<const char *> arg_;
  std::vector<const char *> env_;
  volatile int errCode_;
};

/*
 * NOTE:  We use a singleton to delay construction allowing someone
 * to enable debugging only before the first use of this object.
 */
static NaClDebugState *NaClDebugGetState() {
  static NaClDebugState state;
  return &state;
}

int NaClDebugIsEnabled(void) throw() {
  try {
    return (NDS_ENABLED == NaClDebugGetState()->status_) ? 1 : 0;
  } DBG_CATCH_ALL
}

void NaClDebugSetAppPath(const char *path) throw() {
  try {
    if (NaClDebugIsEnabled()) NaClDebugGetState()->path_ = path;
  } DBG_CATCH_ALL
}


void NaClDebugSetAppInfo(struct NaClApp *app) throw() {
  UNREFERENCED_PARAMETER(app);
}


void NaClDebugSetAppEnvironment(int argc, char const * const argv[],
                                int envc, char const * const envv[]) throw() {
  if (NaClDebugIsEnabled()) {
    int a;
    try {
      /*
       * Copy the pointer arrays.  We use ptrs instead of strings
       * since the data persits and it prevents an extra copy.
       */
      NaClDebugGetState()->arg_.resize(argc);
      for (a = 0; a < argc; a++) NaClDebugGetState()->arg_[a] = argv[a];

      NaClDebugGetState()->env_.resize(envc);
      for (a = 0; a < envc; a++) NaClDebugGetState()->env_[a] = envv[a];
    } DBG_CATCH_ALL
  }
}

void NaClDebugThreadPrepDebugging(struct NaClAppThread *natp) throw() {
  UNREFERENCED_PARAMETER(natp);
}

void NaClDebugThreadStopDebugging(struct NaClAppThread *natp) throw() {
  UNREFERENCED_PARAMETER(natp);
}


int NaClDebugStart() throw() {
  return 0;
}

void NaClDebugStop(int ErrCode) throw() {
  /*
   * We check if debugging is enabled since this check is the only
   * mechanism for allocating the state object.  We free the
   * resources but not the object itself.  Instead we mark it as
   * STOPPED to prevent it from getting recreated.
   */
  if (NaClDebugIsEnabled()) {
    NaClDebugGetState()->status_ = NDS_STOPPED;
    NaClDebugGetState()->errCode_ = ErrCode;
  }
}

