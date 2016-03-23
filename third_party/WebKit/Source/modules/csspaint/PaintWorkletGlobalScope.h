// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintWorkletGlobalScope_h
#define PaintWorkletGlobalScope_h

#include "bindings/core/v8/ScriptValue.h"
#include "core/dom/ExecutionContext.h"
#include "modules/worklet/WorkletGlobalScope.h"

namespace blink {

class PaintWorkletGlobalScope : public WorkletGlobalScope {
    DEFINE_WRAPPERTYPEINFO();
public:
    static PassRefPtrWillBeRawPtr<PaintWorkletGlobalScope> create(LocalFrame*, const KURL&, const String& userAgent, PassRefPtr<SecurityOrigin>, v8::Isolate*);
    ~PaintWorkletGlobalScope() override;

    bool isPaintWorkletGlobalScope() const final { return true; }
    void registerPaint(const String&, ScriptValue) { }

private:
    PaintWorkletGlobalScope(LocalFrame*, const KURL&, const String& userAgent, PassRefPtr<SecurityOrigin>, v8::Isolate*);
};

DEFINE_TYPE_CASTS(PaintWorkletGlobalScope, ExecutionContext, context, context->isPaintWorkletGlobalScope(), context.isPaintWorkletGlobalScope());

} // namespace blink

#endif // PaintWorkletGlobalScope_h
