/*
 * Copyright (C) 2008, 2009, 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "bindings/v8/ScriptValue.h"

#include "bindings/v8/ScriptScope.h"
#include "bindings/v8/SerializedScriptValue.h"
#include "bindings/v8/V8Binding.h"
#include "core/dom/MessagePort.h"
#include "core/platform/JSONValues.h"
#include "wtf/ArrayBuffer.h"

namespace WebCore {

ScriptValue::~ScriptValue()
{
}

PassRefPtr<SerializedScriptValue> ScriptValue::serialize(ScriptState* scriptState)
{
    ScriptScope scope(scriptState);
    return SerializedScriptValue::create(v8Value(), scriptState->isolate());
}

PassRefPtr<SerializedScriptValue> ScriptValue::serialize(ScriptState* scriptState, MessagePortArray* messagePorts, ArrayBufferArray* arrayBuffers, bool& didThrow)
{
    ScriptScope scope(scriptState);
    return SerializedScriptValue::create(v8Value(), messagePorts, arrayBuffers, didThrow, scriptState->isolate());
}

ScriptValue ScriptValue::deserialize(ScriptState* scriptState, SerializedScriptValue* value)
{
    ScriptScope scope(scriptState);
    return ScriptValue(value->deserialize(), scriptState->isolate());
}

bool ScriptValue::getString(String& result, v8::Isolate* isolate) const
{
    if (hasNoValue())
        return false;

    v8::HandleScope handleScope(isolate);
    v8::Handle<v8::Value> string = v8Value();
    if (string.IsEmpty() || !string->IsString())
        return false;
    result = toWebCoreString(string.As<v8::String>());
    return true;
}

String ScriptValue::toString(ScriptState*) const
{
    v8::TryCatch block;
    v8::Handle<v8::String> string = v8Value()->ToString();
    if (block.HasCaught())
        return String();
    return v8StringToWebCoreString<String>(string, DoNotExternalize);
}

static PassRefPtr<JSONValue> v8ToJSONValue(v8::Handle<v8::Value> value, int maxDepth, v8::Isolate* isolate)
{
    if (value.IsEmpty()) {
        ASSERT_NOT_REACHED();
        return 0;
    }

    if (!maxDepth)
        return 0;
    maxDepth--;

    if (value->IsNull() || value->IsUndefined())
        return JSONValue::null();
    if (value->IsBoolean())
        return JSONBasicValue::create(value->BooleanValue());
    if (value->IsNumber())
        return JSONBasicValue::create(value->NumberValue());
    if (value->IsString())
        return JSONString::create(toWebCoreString(value.As<v8::String>()));
    if (value->IsArray()) {
        v8::Handle<v8::Array> array = v8::Handle<v8::Array>::Cast(value);
        RefPtr<JSONArray> inspectorArray = JSONArray::create();
        uint32_t length = array->Length();
        for (uint32_t i = 0; i < length; i++) {
            v8::Local<v8::Value> value = array->Get(v8::Int32::New(i, isolate));
            RefPtr<JSONValue> element = v8ToJSONValue(value, maxDepth, isolate);
            if (!element)
                return 0;
            inspectorArray->pushValue(element);
        }
        return inspectorArray;
    }
    if (value->IsObject()) {
        RefPtr<JSONObject> jsonObject = JSONObject::create();
        v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(value);
        v8::Local<v8::Array> propertyNames = object->GetPropertyNames();
        uint32_t length = propertyNames->Length();
        for (uint32_t i = 0; i < length; i++) {
            v8::Local<v8::Value> name = propertyNames->Get(v8::Int32::New(i, isolate));
            // FIXME(yurys): v8::Object should support GetOwnPropertyNames
            if (name->IsString() && !object->HasRealNamedProperty(v8::Handle<v8::String>::Cast(name)))
                continue;
            RefPtr<JSONValue> propertyValue = v8ToJSONValue(object->Get(name), maxDepth, isolate);
            if (!propertyValue)
                return 0;
            jsonObject->setValue(toWebCoreStringWithNullCheck(name), propertyValue);
        }
        return jsonObject;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

PassRefPtr<JSONValue> ScriptValue::toJSONValue(ScriptState* scriptState) const
{
    v8::HandleScope handleScope(scriptState->isolate());
    // v8::Object::GetPropertyNames() expects current context to be not null.
    v8::Context::Scope contextScope(scriptState->context());
    return v8ToJSONValue(v8Value(), JSONValue::maxDepth, scriptState->isolate());
}

} // namespace WebCore
