// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_CALLBACK_TABLE_
#define NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_CALLBACK_TABLE_

#include <map>

#include "ppapi/c/pp_completion_callback.h"

struct NaClSrpcChannel;

namespace ppapi_proxy {
// Maintains a table of PP_CompletionCallback objects and their respective
// identifiers.  The callback objects can be retrieved using their identifiers.
class CompletionCallbackTable {
 public:
  CompletionCallbackTable();
  ~CompletionCallbackTable() {}

  // Adds the given |callback|, generating and returning an identifier for the
  // |callback|.  If |callback| is empty, then returns 0.
  int32_t AddCallback(const PP_CompletionCallback& callback);
  // Removes the completion callback corresponding to the given |callback_id|
  // and returns it.  If no callback corresponds to the id then an empty
  // PP_CompletionCallback object is returned.
  PP_CompletionCallback RemoveCallback(int32_t callback_id);

  static CompletionCallbackTable& GetTable();

 private:
  std::map<int32_t, PP_CompletionCallback> table_;
  int32_t next_id_;
};

// Data used in the trusted-side of the wire to invoke the completion callback
// on main thread of the untrusted side.
class CallbackInvokerData {
 public:
  CallbackInvokerData(NaClSrpcChannel* channel, int32_t callback_id)
      : channel_(channel), callback_id_(callback_id) {
  }
  int32_t callback_id() const {
    return callback_id_;
  }
  NaClSrpcChannel* channel() const {
    return channel_;
  }

 private:
  NaClSrpcChannel* const channel_;
  const int32_t callback_id_;
};

extern void CompletionCallbackInvoker(void* arg, int32_t);
}  // namespace ppapi_proxy

#endif  // NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_CALLBACK_TABLE_
