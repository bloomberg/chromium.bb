// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HeadersForEachCallback_h
#define HeadersForEachCallback_h

#include "bindings/core/v8/ScriptValue.h"
#include "platform/heap/Handle.h"

namespace blink {

class Headers;

class HeadersForEachCallback : public NoBaseWillBeGarbageCollectedFinalized<HeadersForEachCallback> {
public:
    virtual ~HeadersForEachCallback() { }
    virtual void trace(Visitor*) { }
    virtual bool handleItem(ScriptValue thisValue, const String& value, const String& key, Headers*) = 0;
    virtual bool handleItem(const String& value, const String& key, Headers*) = 0;
};

} // namespace blink

#endif // HeadersForEachCallback_h
