// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UnderlyingSource_h
#define UnderlyingSource_h

#include "platform/heap/Heap.h"

namespace blink {

class ExceptionState;

class UnderlyingSource : public GarbageCollectedFinalized<UnderlyingSource> {
public:
    virtual ~UnderlyingSource() { }
    virtual ScriptPromise startSource(ExceptionState*) = 0;
    virtual void pullSource() = 0;
    virtual void cancelSource() = 0;
    virtual void trace(Visitor*) { }
};

} // namespace blink

#endif // UnderlyingSource_h

