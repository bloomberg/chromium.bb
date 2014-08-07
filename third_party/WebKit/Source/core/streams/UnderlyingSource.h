// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UnderlyingSource_h
#define UnderlyingSource_h

#include "bindings/core/v8/ScriptValue.h"
#include "platform/heap/Heap.h"

namespace blink {

class ExceptionState;
class ScriptState;

class UnderlyingSource : public GarbageCollectedFinalized<UnderlyingSource> {
public:
    virtual ~UnderlyingSource() { }

    // When startSource fails asynchronously, it must call
    // ReadableStream::error with a DOM exception.
    virtual ScriptPromise startSource(ExceptionState*) = 0;
    virtual void pullSource() = 0;
    virtual ScriptPromise cancelSource(ScriptState*, ScriptValue reason) = 0;
    virtual void trace(Visitor*) { }
};

} // namespace blink

#endif // UnderlyingSource_h

