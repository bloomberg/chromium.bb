// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/v8_inspector/public/V8ToProtocolValue.h"

namespace blink {

static String coreString(v8::Local<v8::String> v8String)
{
    int length = v8String->Length();
    UChar* buffer;
    String result = String::createUninitialized(length, buffer);
    v8String->Write(reinterpret_cast<uint16_t*>(buffer), 0, length);
    return result;
}

PassOwnPtr<protocol::Value> toProtocolValue(v8::Local<v8::Context> context, v8::Local<v8::Value> value, int maxDepth)
{
    if (value.IsEmpty()) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    if (!maxDepth)
        return nullptr;
    maxDepth--;

    if (value->IsNull() || value->IsUndefined())
        return protocol::Value::null();
    if (value->IsBoolean())
        return protocol::FundamentalValue::create(value.As<v8::Boolean>()->Value());
    if (value->IsNumber())
        return protocol::FundamentalValue::create(value.As<v8::Number>()->Value());
    if (value->IsString())
        return protocol::StringValue::create(coreString(value.As<v8::String>()));
    if (value->IsArray()) {
        v8::Local<v8::Array> array = value.As<v8::Array>();
        OwnPtr<protocol::ListValue> inspectorArray = protocol::ListValue::create();
        uint32_t length = array->Length();
        for (uint32_t i = 0; i < length; i++) {
            v8::Local<v8::Value> value;
            if (!array->Get(context, i).ToLocal(&value))
                return nullptr;
            OwnPtr<protocol::Value> element = toProtocolValue(context, value, maxDepth);
            if (!element)
                return nullptr;
            inspectorArray->pushValue(element.release());
        }
        return inspectorArray.release();
    }
    if (value->IsObject()) {
        OwnPtr<protocol::DictionaryValue> jsonObject = protocol::DictionaryValue::create();
        v8::Local<v8::Object> object = v8::Local<v8::Object>::Cast(value);
        v8::Local<v8::Array> propertyNames;
        if (!object->GetPropertyNames(context).ToLocal(&propertyNames))
            return nullptr;
        uint32_t length = propertyNames->Length();
        for (uint32_t i = 0; i < length; i++) {
            v8::Local<v8::Value> name;
            if (!propertyNames->Get(context, i).ToLocal(&name))
                return nullptr;
            // FIXME(yurys): v8::Object should support GetOwnPropertyNames
            if (name->IsString()) {
                v8::Maybe<bool> hasRealNamedProperty = object->HasRealNamedProperty(context, v8::Local<v8::String>::Cast(name));
                if (!hasRealNamedProperty.IsJust() || !hasRealNamedProperty.FromJust())
                    continue;
            }
            v8::Local<v8::String> propertyName;
            if (!name->ToString(context).ToLocal(&propertyName))
                continue;
            v8::Local<v8::Value> property;
            if (!object->Get(context, name).ToLocal(&property))
                return nullptr;
            OwnPtr<protocol::Value> propertyValue = toProtocolValue(context, property, maxDepth);
            if (!propertyValue)
                return nullptr;
            jsonObject->setValue(coreString(propertyName), propertyValue.release());
        }
        return jsonObject.release();
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

} // namespace blink
