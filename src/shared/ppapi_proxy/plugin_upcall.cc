// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
//
// This is an early draft of background thread support.
// Until it is complete, we assume that all functions proxy functions
// (but CallOnMainThread) are called on the main thread.
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

namespace {

CompletionCallbackTable* callback_table;
// Guards all insertions and deletions from the above table and upcalls on the
// upcall channel.
pthread_mutex_t upcall_mutex;

class CallbackTableMutexLock {
 public:
  explicit CallbackTableMutexLock(pthread_mutex_t* mutex) : mutex_(mutex) { }
  ~CallbackTableMutexLock() { pthread_mutex_unlock(mutex_); }
 private:
  pthread_mutex_t* mutex_;
};

}  // namespace

namespace ppapi_proxy {

int32_t PluginUpcallStartup() {
  NACL_UNTESTED();
  if (pthread_mutex_init(&upcall_mutex, NULL)) {
    DebugPrintf("PluginUpcallStartup: could not initialize mutex.\n");
    return 0;
  }
  callback_table = new CompletionCallbackTable();
  return 1;
}

void PluginUpcallShutdown() {
  NACL_UNTESTED();
  pthread_mutex_lock(&upcall_mutex);
  delete callback_table;
  callback_table = NULL;
  pthread_mutex_unlock(&upcall_mutex);
  pthread_mutex_destroy(&upcall_mutex);
}

// The call on main thread is implemented via an RPC to the browser side on the
// upcall channel, instead of locally to the plugin. This is to ensure that
// when the callback runs (and potentially calls one of the PPB_ methods
// over RPC), the browser-side is listening.
void PluginUpcallCoreCallOnMainThread(int32_t delay_in_milliseconds,
                                      PP_CompletionCallback callback,
                                      int32_t result) {
  NACL_UNTESTED();
  NaClSrpcChannel* upcall_channel = GetUpcallSrpcChannel();
  if (upcall_channel == NULL) {
    DebugPrintf("PluginUpcallCoreCallOnMainThread: NULL channel.\n");
    return;
  }
  CallbackTableMutexLock ml(&upcall_mutex);
  if (callback_table == NULL) {
    DebugPrintf("PluginUpcallCoreCallOnMainThread: NULL table.\n");
    return;
  }
  int32_t callback_id = callback_table->AddCallback(callback);
  if (callback_id == 0) {
    DebugPrintf("PluginUpcallCoreCallOnMainThread: NULL callback.\n");
    return;
  }
  (void) PppUpcallRpcClient::PPB_Core_CallOnMainThread(
      upcall_channel, delay_in_milliseconds, callback_id, result);
}

}  // namespace ppapi_proxy

// Thread-safe version of CompletionCallbackRpcServer::RunCompletionCallback.
void RunCompletionCallback(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    int32_t callback_id,
    int32_t result) {
  NACL_UNTESTED();
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  PP_CompletionCallback callback = PP_BlockUntilComplete();
  {
    CallbackTableMutexLock ml(&upcall_mutex);
    if (callback_table != NULL) {
      callback = callback_table->RemoveCallback(callback_id);
    }
  }
  if (callback.func == NULL)
    return;
  PP_RunCompletionCallback(&callback, result);
  rpc->result = NACL_SRPC_RESULT_OK;
}
