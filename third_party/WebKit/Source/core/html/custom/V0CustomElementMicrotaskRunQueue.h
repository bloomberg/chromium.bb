// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V0CustomElementMicrotaskRunQueue_h
#define V0CustomElementMicrotaskRunQueue_h

#include "platform/heap/Handle.h"

namespace blink {

class V0CustomElementSyncMicrotaskQueue;
class V0CustomElementAsyncImportMicrotaskQueue;
class V0CustomElementMicrotaskStep;
class HTMLImportLoader;

class V0CustomElementMicrotaskRunQueue
    : public GarbageCollected<V0CustomElementMicrotaskRunQueue> {
 public:
  static V0CustomElementMicrotaskRunQueue* Create() {
    return new V0CustomElementMicrotaskRunQueue;
  }

  void Enqueue(HTMLImportLoader* parent_loader,
               V0CustomElementMicrotaskStep*,
               bool import_is_sync);
  void RequestDispatchIfNeeded();
  bool IsEmpty() const;

  void Trace(blink::Visitor*);

 private:
  V0CustomElementMicrotaskRunQueue();

  void Dispatch();

  Member<V0CustomElementSyncMicrotaskQueue> sync_queue_;
  Member<V0CustomElementAsyncImportMicrotaskQueue> async_queue_;
  bool dispatch_is_pending_;
};

}  // namespace blink

#endif  // V0CustomElementMicrotaskRunQueue_h
