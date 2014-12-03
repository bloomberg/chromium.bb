// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebGLAny_h
#define WebGLAny_h

#include "bindings/core/v8/V8Binding.h"
#include "core/dom/DOMTypedArray.h"
#include "core/html/canvas/WebGLObject.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/text/WTFString.h"

namespace blink {

class WebGLRenderingContextBase;

ScriptValue WebGLAny(ScriptState*);
ScriptValue WebGLAny(ScriptState*, bool value);
ScriptValue WebGLAny(ScriptState*, const bool* value, size_t);
ScriptValue WebGLAny(ScriptState*, int value);
ScriptValue WebGLAny(ScriptState*, unsigned value);
ScriptValue WebGLAny(ScriptState*, float value);
ScriptValue WebGLAny(ScriptState*, String value);
ScriptValue WebGLAny(ScriptState*, PassRefPtrWillBeRawPtr<WebGLObject> value);
ScriptValue WebGLAny(ScriptState*, PassRefPtr<DOMFloat32Array> value);
ScriptValue WebGLAny(ScriptState*, PassRefPtr<DOMInt32Array> value);
ScriptValue WebGLAny(ScriptState*, PassRefPtr<DOMUint8Array> value);
ScriptValue WebGLAny(ScriptState*, PassRefPtr<DOMUint32Array> value);

} // namespace blink

#endif // WebGLAny_h
