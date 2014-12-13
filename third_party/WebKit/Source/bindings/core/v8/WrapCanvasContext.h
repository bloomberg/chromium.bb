// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WrapCanvasContext_h
#define WrapCanvasContext_h

#include "bindings/core/v8/ScriptValue.h"
#include "wtf/PassRefPtr.h"

namespace blink {

class CanvasRenderingContext2D;
class HTMLCanvasElement;
class WebGLRenderingContext;

ScriptValue wrapCanvasContext(ScriptState*, HTMLCanvasElement*, PassRefPtrWillBeRawPtr<CanvasRenderingContext2D> value);
ScriptValue wrapCanvasContext(ScriptState*, HTMLCanvasElement*, PassRefPtrWillBeRawPtr<WebGLRenderingContext> value);

} // namespace blink

#endif // WebGLAny_h
