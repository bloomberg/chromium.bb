// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EventListenerInfo_h
#define EventListenerInfo_h

#include "wtf/Vector.h"
#include "wtf/text/AtomicString.h"

#include <v8.h>

namespace blink {

class EventListenerInfo {
public:
    EventListenerInfo(const AtomicString& eventType, bool useCapture, v8::Local<v8::Object> handler)
        : eventType(eventType)
        , useCapture(useCapture)
        , handler(handler)
    {
    }

    const AtomicString eventType;
    bool useCapture;
    v8::Local<v8::Object> handler;

};

using EventListenerInfoMap = HashMap<String, OwnPtr<Vector<EventListenerInfo>>>;

} // namespace blink

#endif // EventListenerInfo_h
