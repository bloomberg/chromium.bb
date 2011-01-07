// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_CALLBACK_H_
#define NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_CALLBACK_H_

#include <map>

#include "ppapi/c/pp_completion_callback.h"

namespace ppapi_proxy {

// Maintains a table of PP_CompletionCallback objects and their respective
// identifiers that can be used to retrieve the objects.
class CompletionCallbackTable {
 public:
  CompletionCallbackTable();
  ~CompletionCallbackTable() {}

  static CompletionCallbackTable* Get() {
    static CompletionCallbackTable table;
    return &table;
  }

  // Adds the given |callback|, generating and returning an identifier for it.
  // If |callback| is NULL, then returns 0.
  int32_t AddCallback(const PP_CompletionCallback& callback);
  // Removes and returns the callback corresponding to the given |callback_id|.
  // If no callback is found, returns a NULL callback.
  PP_CompletionCallback RemoveCallback(int32_t callback_id);

 private:
  typedef std::map<int32_t, PP_CompletionCallback> CallbackTable;
  CallbackTable table_;
  int32_t next_id_;
};

}  // namespace ppapi_proxy

#endif  // NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_CALLBACK_H_
