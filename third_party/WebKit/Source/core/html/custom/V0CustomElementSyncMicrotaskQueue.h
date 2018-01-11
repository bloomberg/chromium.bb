// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V0CustomElementSyncMicrotaskQueue_h
#define V0CustomElementSyncMicrotaskQueue_h

#include "core/html/custom/V0CustomElementMicrotaskQueueBase.h"

namespace blink {

class V0CustomElementSyncMicrotaskQueue
    : public V0CustomElementMicrotaskQueueBase {
 public:
  static V0CustomElementSyncMicrotaskQueue* Create() {
    return new V0CustomElementSyncMicrotaskQueue();
  }

  void Enqueue(V0CustomElementMicrotaskStep*);

 private:
  V0CustomElementSyncMicrotaskQueue() = default;
  virtual void DoDispatch();
};

}  // namespace blink

#endif  // V0CustomElementSyncMicrotaskQueue_h
