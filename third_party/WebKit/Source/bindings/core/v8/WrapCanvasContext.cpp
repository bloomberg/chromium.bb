// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "bindings/core/v8/WrapCanvasContext.h"

#include "bindings/core/v8/V8CanvasRenderingContext2D.h"
#include "bindings/core/v8/V8WebGLRenderingContext.h"
#include "core/html/HTMLCanvasElement.h"

namespace blink {

ScriptValue wrapCanvasContext(ScriptState* scriptState, HTMLCanvasElement* canvas, PassRefPtrWillBeRawPtr<CanvasRenderingContext2D> value)
{
    v8::Local<v8::Value> v8Result = toV8(value, scriptState->context()->Global(), scriptState->isolate());
    ScriptValue context(scriptState, v8Result);
    return context;
}

ScriptValue wrapCanvasContext(ScriptState* scriptState, HTMLCanvasElement* canvas, PassRefPtrWillBeRawPtr<WebGLRenderingContext> value)
{
    v8::Local<v8::Value> v8Result = toV8(value, scriptState->context()->Global(), scriptState->isolate());
    ScriptValue context(scriptState, v8Result);
    return context;
}

} // namespace blink
