// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectorWebSocketEvents_h
#define InspectorWebSocketEvents_h

#include <memory>
#include "core/inspector/InspectorTraceEvents.h"
#include "platform/heap/Handle.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/instrumentation/tracing/TracedValue.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/Functional.h"

namespace blink {

class Document;
class KURL;

class InspectorWebSocketCreateEvent {
  STATIC_ONLY(InspectorWebSocketCreateEvent);

 public:
  static std::unique_ptr<TracedValue> Data(Document*,
                                           unsigned long identifier,
                                           const KURL&,
                                           const String& protocol);
};

class InspectorWebSocketEvent {
  STATIC_ONLY(InspectorWebSocketEvent);

 public:
  static std::unique_ptr<TracedValue> Data(Document*, unsigned long identifier);
};

}  // namespace blink

#endif  // !defined(InspectorWebSocketEvents_h)
