// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_CALLBACK_TRACKER_H_
#define PPAPI_PROXY_CALLBACK_TRACKER_H_

#include <map>

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_stdint.h"

namespace ppapi {
namespace proxy {

class Dispatcher;

// This object tracks cross-process callbacks. When the plugin sends a callback
// object to the renderer, we save the information and pass an identifier
// instead.
//
// On the renderer side, this identifier is converted to a new callback in that
// process. When executed, this new callback sends an IPC message containing the
// previous identifier back to the plugin.
//
// When we receive that message, ExecuteSerializedCallback converts the
// identifier back to the original callback information and runs the callback.
class CallbackTracker {
 public:
  CallbackTracker(Dispatcher* dispatcher);
  ~CallbackTracker();

  // Converts the given callback in the context of the plugin to a serialized
  // ID. This will be passed to ReceiveCallback on the renderer side.
  uint32_t SendCallback(PP_CompletionCallback callback);

  // Converts the given serialized callback ID to a new completion callback in
  // the context of the current process. This callback actually will represent
  // a proxy that will execute the callback in the plugin process.
  PP_CompletionCallback ReceiveCallback(uint32_t serialized_callback);

  // Sends a request to the remote process to execute the given callback.
  void SendExecuteSerializedCallback(uint32_t serialized_callback,
                                     int32_t param);

  // Executes the given callback ID with the given result in the current
  // process. This will also destroy the information associated with the
  // callback and the serialized ID won't be valid any more.
  void ReceiveExecuteSerializedCallback(uint32_t serialized_callback,
                                        int32_t param);

 private:
  // Pointer to the dispatcher that owns us.
  Dispatcher* dispatcher_;

  int32_t next_callback_id_;

  // Maps callback IDs to the actual callback objects in the plugin process.
  typedef std::map<int32_t, PP_CompletionCallback> CallbackMap;
  CallbackMap callback_map_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_CALLBACK_TRACKER_H_
