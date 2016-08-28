// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StringUtil_h
#define StringUtil_h

#include "platform/v8_inspector/Allocator.h"
#include "platform/v8_inspector/String16.h"
#include "platform/v8_inspector/public/StringBuffer.h"
#include "platform/v8_inspector/public/StringView.h"

#include <v8.h>

namespace v8_inspector {

namespace protocol {

class Value;

using String = v8_inspector::String16;
using StringBuilder = v8_inspector::String16Builder;

class StringUtil {
public:
    static String substring(const String& s, unsigned pos, unsigned len) { return s.substring(pos, len); }
    static String fromInteger(int number) { return String::fromInteger(number); }
    static String fromDouble(double number) { return String::fromDouble(number); }
    static const size_t kNotFound = String::kNotFound;
    static void builderReserve(StringBuilder& builder, unsigned capacity) { builder.reserveCapacity(capacity); }
};

std::unique_ptr<protocol::Value> parseJSON(const StringView& json);
std::unique_ptr<protocol::Value> parseJSON(const String16& json);

} // namespace protocol

std::unique_ptr<protocol::Value> toProtocolValue(v8::Local<v8::Context>, v8::Local<v8::Value>, int maxDepth = 1000);

v8::Local<v8::String> toV8String(v8::Isolate*, const String16&);
v8::Local<v8::String> toV8StringInternalized(v8::Isolate*, const String16&);
v8::Local<v8::String> toV8StringInternalized(v8::Isolate*, const char*);
v8::Local<v8::String> toV8String(v8::Isolate*, const StringView&);
// TODO(dgozman): rename to toString16.
String16 toProtocolString(v8::Local<v8::String>);
String16 toProtocolStringWithTypeCheck(v8::Local<v8::Value>);
String16 toString16(const StringView&);
StringView toStringView(const String16&);
bool stringViewStartsWith(const StringView&, const char*);

class StringBufferImpl : public StringBuffer {
    V8_INSPECTOR_DISALLOW_COPY(StringBufferImpl);
public:
    // Destroys string's content.
    static std::unique_ptr<StringBufferImpl> adopt(String16&);
    const StringView& string() override { return m_string; }

private:
    explicit StringBufferImpl(String16&);
    String16 m_owner;
    StringView m_string;
};

} //  namespace v8_inspector


#endif // !defined(StringUtil_h)
