// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
//
// This is an early draft of background thread support.
// Until it is complete, we assume that all proxy functions
// (except CallOnMainThread) are called on the main PPAPI thread.
//
// WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING

#include "native_client/src/shared/ppapi_proxy/plugin_upcall.h"

#include <pthread.h>
#include <map>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/plugin_callback.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "ppapi/c/pp_errors.h"
#include "srpcgen/ppp_rpc.h"
#include "srpcgen/upcall.h"

using ppapi_proxy::CompletionCallbackTable;

namespace ppapi_proxy {

namespace {

  class CallOnMainThreadCriticalSection {
    static pthread_mutex_t mutex_;
   public:
    CallOnMainThreadCriticalSection() { pthread_mutex_lock(&mutex_); }
    ~CallOnMainThreadCriticalSection() { pthread_mutex_unlock(&mutex_); }
  };

  pthread_mutex_t CallOnMainThreadCriticalSection::mutex_ =
      PTHREAD_MUTEX_INITIALIZER;

}  // namespace

// The call on main thread is implemented via an RPC to the browser side on the
// upcall channel, instead of locally to the plugin. This is to ensure that
// when the callback runs (and potentially calls one of the PPB_ methods
// over RPC), the browser-side is listening.
void PluginUpcallCoreCallOnMainThread(int32_t delay_in_milliseconds,
                                      PP_CompletionCallback callback,
                                      int32_t result) {
  // Force PluginUpcallCoreCallOnMainThread, from multiple threads, to occur
  // one at a time.
  CallOnMainThreadCriticalSection guard;
  NaClSrpcChannel* upcall_channel = GetUpcallSrpcChannel();
  if (upcall_channel == NULL) {
    DebugPrintf("PluginUpcallCoreCallOnMainThread: NULL channel.\n");
    return;
  }
  int32_t callback_id =
      ppapi_proxy::CompletionCallbackTable::Get()->AddCallback(callback);
  if (callback_id == 0) {
    DebugPrintf("PluginUpcallCoreCallOnMainThread: NULL callback.\n");
    return;
  }
  (void) PppUpcallRpcClient::PPB_Core_CallOnMainThread(
      upcall_channel, delay_in_milliseconds, callback_id, result);
}

}  // namespace ppapi_proxy
