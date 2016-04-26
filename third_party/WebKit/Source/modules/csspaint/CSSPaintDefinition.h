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

class Image;
class ScriptState;

// Represents a javascript class registered on the PaintWorkletGlobalScope by
// the author.
class CSSPaintDefinition final : public GarbageCollectedFinalized<CSSPaintDefinition> {
public:
    static CSSPaintDefinition* create(ScriptState*, v8::Local<v8::Function> constructor, v8::Local<v8::Function> paint);
    virtual ~CSSPaintDefinition();

    // Invokes the javascript 'paint' callback on an instance of the javascript
    // class. The size given will be the size of the PaintRenderingContext2D
    // given to the callback.
    //
    // This may return a nullptr (representing an invalid image) if javascript
    // throws an error.
    PassRefPtr<Image> paint(const IntSize&);

    ScriptState* getScriptState() const { return m_scriptState.get(); }

    v8::Local<v8::Function> paintFunctionForTesting(v8::Isolate* isolate) { return m_paint.newLocal(isolate); }

    DEFINE_INLINE_TRACE() { };

private:
    CSSPaintDefinition(ScriptState*, v8::Local<v8::Function> constructor, v8::Local<v8::Function> paint);

    void maybeCreatePaintInstance();

    RefPtr<ScriptState> m_scriptState;

    // This object keeps the class instance object, constructor function and
    // paint function alive. This object needs to be destroyed to break a
    // reference cycle between it and the PaintWorkletGlobalScope.
    ScopedPersistent<v8::Function> m_constructor;
    ScopedPersistent<v8::Function> m_paint;

    // At the moment there is only ever one instance of a paint class per type.
    ScopedPersistent<v8::Object> m_instance;

    bool m_didCallConstructor;
};

} // namespace blink

#endif // CSSPaintDefinition_h
