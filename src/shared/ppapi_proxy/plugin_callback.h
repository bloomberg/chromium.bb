// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_CALLBACK_H_
#define NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_CALLBACK_H_

#include <pthread.h>
#include <map>
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "ppapi/c/pp_completion_callback.h"

namespace ppapi_proxy {

// Skips callback invocation and returns |result| if callback function is NULL
// or PP_COMPLETIONCALLBACK_FLAG_OPTIONAL is set. Otherwise, schedules the
// callback with |result| as an argument and returns PP_OK_COMPLETIONPENDING.
int32_t MayForceCallback(PP_CompletionCallback callback, int32_t result);

// Maintains a table of PP_CompletionCallback objects and their respective
// identifiers that can be used to retrieve the objects.
class CompletionCallbackTable {
 public:
  // Return a singleton instance.
  static CompletionCallbackTable* Get() {
    static CompletionCallbackTable table;
    return &table;
  }

  // Adds the given |callback| and optionally the associated |read_buffer|,
  // generating and returning an identifier for it.
  // If |callback| is NULL, then returns 0.
  int32_t AddCallback(const PP_CompletionCallback& callback);
  int32_t AddCallback(const PP_CompletionCallback& callback, void* read_buffer);
  // Removes and returns the callback and optionally the associated
  // |read_buffer| corresponding to the given |callback_id|.
  // If no callback is found, returns a NULL callback.
  PP_CompletionCallback RemoveCallback(int32_t callback_id, void** read_buffer);

 private:
  // Currently implemented as singleton, so use a private constructor.
  CompletionCallbackTable() : next_id_(1) { }
  ~CompletionCallbackTable() { }

  struct CallbackInfo {
    PP_CompletionCallback callback;
    void* read_buffer;  // To be used with callbacks invoked on byte reads.
  };

  typedef std::map<int32_t, CallbackInfo> CallbackTable;
  CallbackTable table_;
  int32_t next_id_;

  // Single static mutex used as critical section for all callback tables.
  static pthread_mutex_t mutex_;
  class CallbackTableCriticalSection {
   public:
    CallbackTableCriticalSection() { pthread_mutex_lock(&mutex_); }
    ~CallbackTableCriticalSection() { pthread_mutex_unlock(&mutex_); }
  };
};

}  // namespace ppapi_proxy

#endif  // NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_CALLBACK_H_
