// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebGLAny_h
#define WebGLAny_h

#include "base/memory/scoped_refptr.h"
#include "bindings/core/v8/ScriptValue.h"
#include "core/typed_arrays/DOMTypedArray.h"
#include "modules/webgl/WebGLObject.h"
#include "platform/wtf/Forward.h"

namespace blink {

ScriptValue WebGLAny(ScriptState*, bool value);
ScriptValue WebGLAny(ScriptState*, const bool* value, size_t);
ScriptValue WebGLAny(ScriptState*, const Vector<bool>& value);
ScriptValue WebGLAny(ScriptState*, const Vector<unsigned>& value);
ScriptValue WebGLAny(ScriptState*, const Vector<int>& value);
ScriptValue WebGLAny(ScriptState*, int value);
ScriptValue WebGLAny(ScriptState*, unsigned value);
ScriptValue WebGLAny(ScriptState*, int64_t value);
ScriptValue WebGLAny(ScriptState*, uint64_t value);
ScriptValue WebGLAny(ScriptState*, float value);
ScriptValue WebGLAny(ScriptState*, String value);
ScriptValue WebGLAny(ScriptState*, WebGLObject* value);
ScriptValue WebGLAny(ScriptState*, DOMFloat32Array* value);
ScriptValue WebGLAny(ScriptState*, DOMInt32Array* value);
ScriptValue WebGLAny(ScriptState*, DOMUint8Array* value);
ScriptValue WebGLAny(ScriptState*, DOMUint32Array* value);

}  // namespace blink

#endif  // WebGLAny_h
