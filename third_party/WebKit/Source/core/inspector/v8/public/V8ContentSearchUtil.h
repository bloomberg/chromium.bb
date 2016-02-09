// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8ContentSearchUtil_h
#define V8ContentSearchUtil_h

#include "core/InspectorTypeBuilder.h"

#include "wtf/text/WTFString.h"

namespace blink {

class V8Debugger;

namespace V8ContentSearchUtil {

String findSourceURL(const String& content, bool multiline);
String findSourceMapURL(const String& content, bool multiline);
PassRefPtr<TypeBuilder::Array<TypeBuilder::Debugger::SearchMatch>> searchInTextByLines(V8Debugger*, const String& text, const String& query, const bool caseSensitive, const bool isRegex);

}

}

#endif // !defined(V8ContentSearchUtil_h)
