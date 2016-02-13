// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8EventListenerInfo_h
#define V8EventListenerInfo_h

#include "wtf/Vector.h"
#include "wtf/text/AtomicString.h"

#include <v8.h>

namespace blink {

class V8EventListenerInfo {
public:
    V8EventListenerInfo(const AtomicString& eventType, bool useCapture, v8::Local<v8::Object> handler)
        : eventType(eventType)
        , useCapture(useCapture)
        , handler(handler)
    {
    }

    const AtomicString eventType;
    bool useCapture;
    v8::Local<v8::Object> handler;

};

using V8EventListenerInfoMap = HashMap<String, OwnPtr<Vector<V8EventListenerInfo>>>;

} // namespace blink

#endif // V8EventListenerInfo_h
