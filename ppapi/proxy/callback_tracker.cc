// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/callback_tracker.h"

#include "ppapi/proxy/dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"

namespace ppapi {
namespace proxy {

namespace {

struct CallbackData {
  CallbackTracker* tracker;
  uint32_t callback_id;
};

void CallbackProxy(void* user_data, int32_t result) {
  CallbackData* data = static_cast<CallbackData*>(user_data);
  data->tracker->SendExecuteSerializedCallback(data->callback_id, result);
  delete data;
}

}  // namespace

CallbackTracker::CallbackTracker(Dispatcher* dispatcher)
    : dispatcher_(dispatcher),
      next_callback_id_(1) {
}

CallbackTracker::~CallbackTracker() {
}

uint32_t CallbackTracker::SendCallback(PP_CompletionCallback callback) {
  // Find the next callback ID we can use (being careful about wraparound).
  while (callback_map_.find(next_callback_id_) != callback_map_.end())
    next_callback_id_++;
  callback_map_[next_callback_id_] = callback;
  return next_callback_id_++;
}

PP_CompletionCallback CallbackTracker::ReceiveCallback(
    uint32_t serialized_callback) {
  CallbackData* data = new CallbackData;
  data->tracker = this;
  data->callback_id = serialized_callback;
  return PP_MakeCompletionCallback(&CallbackProxy, data);
}

void CallbackTracker::SendExecuteSerializedCallback(
    uint32_t serialized_callback,
    int32_t param) {
  dispatcher_->Send(new PpapiMsg_ExecuteCallback(serialized_callback, param));
}

void CallbackTracker::ReceiveExecuteSerializedCallback(
    uint32_t serialized_callback,
    int32_t param) {
  CallbackMap::iterator found = callback_map_.find(serialized_callback);
  if (found == callback_map_.end()) {
    NOTREACHED();
    return;
  }

  PP_RunCompletionCallback(&found->second, param);
  callback_map_.erase(found);
}

}  // namespace proxy
}  // namespace ppapi
