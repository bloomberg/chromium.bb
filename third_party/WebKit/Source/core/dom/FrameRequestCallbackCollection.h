// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FrameRequestCallbackCollection_h
#define FrameRequestCallbackCollection_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"

namespace blink {

class ExecutionContext;
class FrameRequestCallback;

class CORE_EXPORT FrameRequestCallbackCollection final {
  DISALLOW_NEW();

 public:
  explicit FrameRequestCallbackCollection(ExecutionContext*);

  using CallbackId = int;
  CallbackId RegisterCallback(FrameRequestCallback*);
  void CancelCallback(CallbackId);
  void ExecuteCallbacks(double high_res_now_ms, double high_res_now_ms_legacy);

  bool IsEmpty() const { return !callbacks_.size(); }

  DECLARE_TRACE();

 private:
  using CallbackList = HeapVector<Member<FrameRequestCallback>>;
  CallbackList callbacks_;
  CallbackList
      callbacks_to_invoke_;  // only non-empty while inside executeCallbacks

  CallbackId next_callback_id_ = 0;

  Member<ExecutionContext> context_;
};

}  // namespace blink

#endif  // FrameRequestCallbackCollection_h
