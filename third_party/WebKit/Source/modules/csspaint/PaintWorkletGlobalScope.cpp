// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/csspaint/PaintWorkletGlobalScope.h"

#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "core/inspector/MainThreadDebugger.h"

namespace blink {

// static
PassRefPtrWillBeRawPtr<PaintWorkletGlobalScope> PaintWorkletGlobalScope::create(LocalFrame* frame, const KURL& url, const String& userAgent, PassRefPtr<SecurityOrigin> securityOrigin, v8::Isolate* isolate)
{
    RefPtrWillBeRawPtr<PaintWorkletGlobalScope> paintWorkletGlobalScope = adoptRefWillBeNoop(new PaintWorkletGlobalScope(frame, url, userAgent, securityOrigin, isolate));
    paintWorkletGlobalScope->scriptController()->initializeContextIfNeeded();
    MainThreadDebugger::contextCreated(paintWorkletGlobalScope->scriptController()->getScriptState(), paintWorkletGlobalScope->frame(), paintWorkletGlobalScope->getSecurityOrigin());
    return paintWorkletGlobalScope.release();
}

PaintWorkletGlobalScope::PaintWorkletGlobalScope(LocalFrame* frame, const KURL& url, const String& userAgent, PassRefPtr<SecurityOrigin> securityOrigin, v8::Isolate* isolate)
    : WorkletGlobalScope(frame, url, userAgent, securityOrigin, isolate)
{
}

PaintWorkletGlobalScope::~PaintWorkletGlobalScope()
{
}

} // namespace blink
