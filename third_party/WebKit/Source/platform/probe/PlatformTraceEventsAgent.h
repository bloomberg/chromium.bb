// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PlatformTraceEventsAgent_h
#define PlatformTraceEventsAgent_h

#include "platform/PlatformExport.h"
#include "platform/heap/Heap.h"

namespace blink {

namespace probe {
class PlatformSendRequest;
}

class PLATFORM_EXPORT PlatformTraceEventsAgent
    : public GarbageCollected<PlatformTraceEventsAgent> {
 public:
  void Trace(blink::Visitor* visitor) {}

  void Will(const probe::PlatformSendRequest&);
  void Did(const probe::PlatformSendRequest&);
};

}  // namespace blink

#endif  // !defined(PlatformTraceEventsAgent_h)
