/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "bindings/core/v8/custom/V8ArrayBufferViewCustom.h"

#include "bindings/core/v8/custom/V8DataViewCustom.h"
#include "bindings/core/v8/custom/V8Float32ArrayCustom.h"
#include "bindings/core/v8/custom/V8Float64ArrayCustom.h"
#include "bindings/core/v8/custom/V8Int16ArrayCustom.h"
#include "bindings/core/v8/custom/V8Int32ArrayCustom.h"
#include "bindings/core/v8/custom/V8Int8ArrayCustom.h"
#include "bindings/core/v8/custom/V8Uint16ArrayCustom.h"
#include "bindings/core/v8/custom/V8Uint32ArrayCustom.h"
#include "bindings/core/v8/custom/V8Uint8ArrayCustom.h"
#include "bindings/core/v8/custom/V8Uint8ClampedArrayCustom.h"
#include <v8.h>

namespace blink {

using namespace WTF;

ArrayBufferView* V8ArrayBufferView::toImpl(v8::Handle<v8::Object> object)
{
    ASSERT(object->IsArrayBufferView());
    ScriptWrappableBase* viewPtr = blink::toScriptWrappableBase(object);
    if (viewPtr)
        return reinterpret_cast<ArrayBufferView*>(viewPtr);

    if (object->IsUint8Array()) {
        return V8Uint8Array::toImpl(object);
    }
    if (object->IsInt8Array()) {
        return V8Int8Array::toImpl(object);
    }
    if (object->IsUint16Array()) {
        return V8Uint16Array::toImpl(object);
    }
    if (object->IsInt16Array()) {
        return V8Int16Array::toImpl(object);
    }
    if (object->IsUint32Array()) {
        return V8Uint32Array::toImpl(object);
    }
    if (object->IsInt32Array()) {
        return V8Int32Array::toImpl(object);
    }
    if (object->IsFloat32Array()) {
        return V8Float32Array::toImpl(object);
    }
    if (object->IsFloat64Array()) {
        return V8Float64Array::toImpl(object);
    }
    if (object->IsUint8ClampedArray()) {
        return V8Uint8ClampedArray::toImpl(object);
    }
    if (object->IsDataView()) {
        return V8DataView::toImpl(object);
    }
    ASSERT_NOT_REACHED();
    return 0;
}

ArrayBufferView* V8ArrayBufferView::toImplWithTypeCheck(v8::Isolate* isolate, v8::Handle<v8::Value> value)
{
    return V8ArrayBufferView::hasInstance(value, isolate) ? V8ArrayBufferView::toImpl(v8::Handle<v8::Object>::Cast(value)) : 0;
}

} // namespace blink
