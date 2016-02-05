// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8StringUtil_h
#define V8StringUtil_h

#include "core/InspectorTypeBuilder.h"

#include "wtf/text/WTFString.h"
#include <v8.h>

namespace blink {

class V8Debugger;

v8::Local<v8::String> toV8String(v8::Isolate*, const String&);
v8::Local<v8::String> toV8StringInternalized(v8::Isolate*, const String&);

String toWTFString(v8::Local<v8::String>);
String toWTFStringWithTypeCheck(v8::Local<v8::Value>);

namespace V8StringUtil {

String findSourceURL(const String& content, bool multiline);
String findSourceMapURL(const String& content, bool multiline);
PassRefPtr<TypeBuilder::Array<TypeBuilder::Debugger::SearchMatch>> searchInTextByLines(V8Debugger*, const String& text, const String& query, const bool caseSensitive, const bool isRegex);

} //  namespace V8StringUtil

} //  namespace blink


#endif // !defined(V8StringUtil_h)
