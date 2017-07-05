// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V0CustomElementMicrotaskQueueBase_h
#define V0CustomElementMicrotaskQueueBase_h

#include "core/html/custom/V0CustomElementMicrotaskStep.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Vector.h"

namespace blink {

class V0CustomElementMicrotaskQueueBase
    : public GarbageCollectedFinalized<V0CustomElementMicrotaskQueueBase> {
  WTF_MAKE_NONCOPYABLE(V0CustomElementMicrotaskQueueBase);

 public:
  virtual ~V0CustomElementMicrotaskQueueBase() {}

  bool IsEmpty() const { return queue_.IsEmpty(); }
  void Dispatch();

  DECLARE_TRACE();

#if !defined(NDEBUG)
  void Show(unsigned indent);
#endif

 protected:
  V0CustomElementMicrotaskQueueBase() : in_dispatch_(false) {}
  virtual void DoDispatch() = 0;

  HeapVector<Member<V0CustomElementMicrotaskStep>> queue_;
  bool in_dispatch_;
};

}  // namespace blink

#endif  // V0CustomElementMicrotaskQueueBase_h
