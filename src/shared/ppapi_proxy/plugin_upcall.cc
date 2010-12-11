// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/plugin_upcall.h"

#include <pthread.h>
#include <map>

#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "ppapi/c/pp_errors.h"
#include "srpcgen/ppp_rpc.h"
#include "srpcgen/upcall.h"

namespace {

typedef int32_t CallbackIndex;

// Maintains a table of PP_CompletionCallback objects and their respective
// identifiers.  The callback objects can be retrieved using their identifiers.
class CompletionCallbackTable {
 public:
  CompletionCallbackTable();
  ~CompletionCallbackTable() {}

  // Adds the given |callback|, generating and returning an identifier for the
  // |callback|.  If |callback| is empty, then returns 0.
  CallbackIndex AddCallback(const PP_CompletionCallback& callback);
  // Removes the completion callback corresponding to the given |callback_id|
  // and returns it.  If no callback corresponds to the id then an empty
  // PP_CompletionCallback object is returned.
  PP_CompletionCallback RemoveCallback(CallbackIndex callback_id);

 private:
  std::map<CallbackIndex, PP_CompletionCallback> table_;
  CallbackIndex next_id_;
};

CompletionCallbackTable::CompletionCallbackTable()
  : next_id_(0) {
}

CallbackIndex CompletionCallbackTable::AddCallback(
    const PP_CompletionCallback& callback) {
  if (callback.func == NULL)
    return 0;
  table_[next_id_] = callback;
  return next_id_++;
}

PP_CompletionCallback CompletionCallbackTable::RemoveCallback(
    CallbackIndex callback_id) {
  std::map<CallbackIndex, PP_CompletionCallback>::iterator it =
      table_.find(callback_id);
  if (table_.end() != it) {
    return it->second;
  }
  PP_CompletionCallback rv = { NULL, NULL };
  return rv;
}

CompletionCallbackTable* callback_table;

// Guards all insertions and deletions from the table and upcalls on the
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
  if (pthread_mutex_init(&upcall_mutex, NULL)) {
    DebugPrintf("PluginUpcallStartup: could not initialize mutex.\n");
    return 0;
  }
  callback_table = new CompletionCallbackTable();
  return 1;
}

void PluginUpcallShutdown() {
  pthread_mutex_lock(&upcall_mutex);
  delete callback_table;
  callback_table = NULL;
  pthread_mutex_unlock(&upcall_mutex);
  pthread_mutex_destroy(&upcall_mutex);
}

void PluginUpcallCoreCallOnMainThread(int32_t delay_in_milliseconds,
                                      PP_CompletionCallback callback,
                                      int32_t result) {
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
  CallbackIndex closure_number = callback_table->AddCallback(callback);
  (void) PppUpcallRpcClient::PPB_Core_CallOnMainThread(upcall_channel,
                                                       closure_number,
                                                       delay_in_milliseconds);
}

int32_t PluginUpcallGraphics2DFlush(PP_Resource graphics_2d,
                                    struct PP_CompletionCallback callback) {
  NaClSrpcChannel* upcall_channel = GetUpcallSrpcChannel();
  if (upcall_channel == NULL) {
    DebugPrintf("PluginUpcallGraphics2DFlush: NULL channel.\n");
    return PP_ERROR_FAILED;
  }
  CallbackTableMutexLock ml(&upcall_mutex);
  if (callback_table == NULL) {
    DebugPrintf("PluginUpcallGraphics2DFlush: NULL table.\n");
    return PP_ERROR_FAILED;
  }
  CallbackIndex closure_number = callback_table->AddCallback(callback);
  int32_t success;
  NaClSrpcError retval =
      PppUpcallRpcClient::PPB_Graphics2D_Flush(
          upcall_channel,
          static_cast<int64_t>(graphics_2d),
          closure_number,
          &success);
  if (retval != NACL_SRPC_RESULT_OK) {
    return PP_ERROR_FAILED;
  }
  return success;
}

}  // namespace ppapi_proxy

// Invokes the PP_CompletionCallback in the plugin-side after it was invoked via
// srpc from the browser-side.
void CompletionCallbackRpcServer::InvokeCompletionCallback(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* closure,
      int32_t callback_id,
      int32_t result) {
  NaClSrpcClosureRunner runner(closure);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  PP_CompletionCallback callback = { NULL, NULL };
  {
    CallbackTableMutexLock ml(&upcall_mutex);
    if (callback_table != NULL) {
      callback = callback_table->RemoveCallback(callback_id);
    }
  }
  if (NULL != callback.func) {
    (*callback.func)(callback.user_data, result);
    rpc->result = NACL_SRPC_RESULT_OK;
  }
}

