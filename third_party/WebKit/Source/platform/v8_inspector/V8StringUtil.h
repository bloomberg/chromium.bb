// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8StringUtil_h
#define V8StringUtil_h

#include "platform/inspector_protocol/InspectorProtocol.h"
#include "platform/v8_inspector/protocol/Debugger.h"
#include "platform/v8_inspector/public/StringBuffer.h"
#include "platform/v8_inspector/public/StringView.h"
#include <v8.h>

namespace v8_inspector {

class V8InspectorSession;

namespace protocol = blink::protocol;

std::unique_ptr<protocol::Value> toProtocolValue(v8::Local<v8::Context>, v8::Local<v8::Value>, int maxDepth = protocol::Value::maxDepth);

v8::Local<v8::String> toV8String(v8::Isolate*, const String16&);
v8::Local<v8::String> toV8StringInternalized(v8::Isolate*, const String16&);
v8::Local<v8::String> toV8StringInternalized(v8::Isolate*, const char*);
v8::Local<v8::String> toV8String(v8::Isolate*, const StringView&);

String16 toProtocolString(v8::Local<v8::String>);
String16 toProtocolStringWithTypeCheck(v8::Local<v8::Value>);
String16 toString16(const StringView&);
StringView toStringView(const String16&);

bool stringViewStartsWith(const StringView&, const char*);
std::unique_ptr<protocol::Value> parseJSON(const StringView& json);

String16 findSourceURL(const String16& content, bool multiline);
String16 findSourceMapURL(const String16& content, bool multiline);
std::vector<std::unique_ptr<protocol::Debugger::SearchMatch>> searchInTextByLinesImpl(V8InspectorSession*, const String16& text, const String16& query, bool caseSensitive, bool isRegex);

class StringBufferImpl : public StringBuffer {
    PROTOCOL_DISALLOW_COPY(StringBufferImpl);
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


#endif // !defined(V8StringUtil_h)
