// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Iterator_h
#define Iterator_h

#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "platform/heap/Handle.h"

namespace blink {

class ExceptionState;

class Iterator : public GarbageCollectedFinalized<Iterator>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    Iterator() { ScriptWrappable::init(this); }
    virtual ~Iterator() { }

    virtual ScriptValue next(ScriptState*, ExceptionState&) = 0;
    virtual ScriptValue next(ScriptState*, ScriptValue /* value */, ExceptionState&) = 0;
    Iterator* iterator(ScriptState*, ExceptionState&) { return this; }

    virtual void trace(Visitor*) { }
};

} // namespace blink

#endif

