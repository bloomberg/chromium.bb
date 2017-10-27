// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VRFrameRequestCallbackCollection_h
#define VRFrameRequestCallbackCollection_h

#include "platform/bindings/TraceWrapperMember.h"
#include "platform/heap/Handle.h"

namespace blink {

class ExecutionContext;
class V8VRFrameRequestCallback;
class VRPresentationFrame;
class VRSession;

class VRFrameRequestCallbackCollection final : public TraceWrapperBase {
  DISALLOW_NEW();

 public:
  explicit VRFrameRequestCallbackCollection(ExecutionContext*);

  using CallbackId = int;
  CallbackId RegisterCallback(V8VRFrameRequestCallback*);
  void CancelCallback(CallbackId);
  void ExecuteCallbacks(VRSession*, VRPresentationFrame*);

  bool IsEmpty() const { return !callbacks_.size(); }

  void Trace(blink::Visitor*);
  void TraceWrappers(const blink::ScriptWrappableVisitor*) const;

 private:
  bool IsValidCallbackId(int id) {
    using Traits = HashTraits<CallbackId>;
    return !Traits::IsDeletedValue(id) &&
           !WTF::IsHashTraitsEmptyValue<Traits, CallbackId>(id);
  }

  using CallbackMap =
      HeapHashMap<CallbackId, TraceWrapperMember<V8VRFrameRequestCallback>>;
  CallbackMap callbacks_;
  Vector<CallbackId> pending_callbacks_;
  // Only non-empty while inside executeCallbacks.
  Vector<CallbackId> callbacks_to_invoke_;

  CallbackId next_callback_id_ = 0;

  Member<ExecutionContext> context_;
};

}  // namespace blink

#endif  // FrameRequestCallbackCollection_h
