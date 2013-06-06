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
#include "bindings/v8/custom/V8ArrayBufferViewCustom.h"

#include "V8ArrayBufferViewCustomScript.h"
#include "bindings/v8/V8HiddenPropertyName.h"
#include "bindings/v8/V8ScriptRunner.h"

#include <v8.h>

namespace WebCore {

bool copyElements(v8::Handle<v8::Object> destArray, v8::Handle<v8::Object> srcArray, uint32_t length, uint32_t offset, v8::Isolate* isolate)
{
    v8::Handle<v8::Value> prototype_value = destArray->GetPrototype();
    if (prototype_value.IsEmpty() || !prototype_value->IsObject())
        return false;
    v8::Handle<v8::Object> prototype = prototype_value.As<v8::Object>();
    v8::Handle<v8::Value> value = prototype->GetHiddenValue(V8HiddenPropertyName::typedArrayHiddenCopyMethod());
    if (value.IsEmpty()) {
        String source(reinterpret_cast<const char*>(V8ArrayBufferViewCustomScript_js), sizeof(V8ArrayBufferViewCustomScript_js));
        value = V8ScriptRunner::compileAndRunInternalScript(v8String(source, isolate), isolate);
        prototype->SetHiddenValue(V8HiddenPropertyName::typedArrayHiddenCopyMethod(), value);
    }
    if (value.IsEmpty() || !value->IsFunction())
        return false;
    v8::Handle<v8::Function> copy_method = value.As<v8::Function>();
    v8::Handle<v8::Value> arguments[3] = { srcArray, v8::Uint32::New(length), v8::Uint32::New(offset) };
    V8ScriptRunner::callInternalFunction(copy_method, destArray, WTF_ARRAY_LENGTH(arguments), arguments, isolate);
    return true;
}

}
