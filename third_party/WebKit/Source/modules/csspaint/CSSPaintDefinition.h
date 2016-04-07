// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPaintDefinition_h
#define CSSPaintDefinition_h

#include "bindings/core/v8/ScopedPersistent.h"
#include "platform/geometry/IntSize.h"
#include "platform/heap/Handle.h"
#include <v8.h>

namespace blink {

class ScriptState;

class CSSPaintDefinition final : public GarbageCollectedFinalized<CSSPaintDefinition> {
public:
    static CSSPaintDefinition* create(ScriptState*, v8::Local<v8::Function> constructor, v8::Local<v8::Function> paint);
    virtual ~CSSPaintDefinition();

    v8::Local<v8::Function> paintFunctionForTesting(v8::Isolate* isolate) { return m_paint.newLocal(isolate); }

    DEFINE_INLINE_TRACE() { };

private:
    CSSPaintDefinition(ScriptState*, v8::Local<v8::Function> constructor, v8::Local<v8::Function> paint);

    RefPtr<ScriptState> m_scriptState;

    // This object keeps the constructor and paint functions alive. This object
    // needs to be destroyed to break a reference cycle between it and the
    // PaintWorkletGlobalScope.
    ScopedPersistent<v8::Function> m_constructor;
    ScopedPersistent<v8::Function> m_paint;
};

} // namespace blink

#endif // CSSPaintDefinition_h
