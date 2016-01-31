// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/inspector/v8/V8StringUtil.h"

namespace blink {

v8::Local<v8::String> toV8String(v8::Isolate* isolate, const String& string)
{
    return v8::String::NewFromUtf8(isolate, string.utf8().data(), v8::NewStringType::kNormal).ToLocalChecked();
}

v8::Local<v8::String> toV8StringInternalized(v8::Isolate* isolate, const String& string)
{
    return v8::String::NewFromUtf8(isolate, string.utf8().data(), v8::NewStringType::kInternalized).ToLocalChecked();
}

String toWTFString(v8::Local<v8::String> value)
{
    if (value.IsEmpty() || value->IsNull() || value->IsUndefined())
        return String();
    UChar* buffer;
    String result = String::createUninitialized(value->Length(), buffer);
    value->Write(reinterpret_cast<uint16_t*>(buffer), 0, value->Length());
    return result;
}

String toWTFStringWithTypeCheck(v8::Local<v8::Value> value)
{
    if (value.IsEmpty() || !value->IsString())
        return String();
    return toWTFString(value.As<v8::String>());
}

} // namespace blink
