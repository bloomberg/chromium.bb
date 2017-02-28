// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8EventListenerInfo_h
#define V8EventListenerInfo_h

#include "v8/include/v8.h"
#include "wtf/Vector.h"
#include "wtf/text/AtomicString.h"

namespace blink {

class V8EventListenerInfo {
 public:
  V8EventListenerInfo(AtomicString eventType,
                      bool useCapture,
                      bool passive,
                      bool once,
                      v8::Local<v8::Object> handler,
                      int backendNodeId)
      : eventType(eventType),
        useCapture(useCapture),
        passive(passive),
        once(once),
        handler(handler),
        backendNodeId(backendNodeId) {}

  AtomicString eventType;
  bool useCapture;
  bool passive;
  bool once;
  v8::Local<v8::Object> handler;
  int backendNodeId;
};

using V8EventListenerInfoList = Vector<V8EventListenerInfo>;

}  // namespace blink

#endif  // V8EventListenerInfo_h
