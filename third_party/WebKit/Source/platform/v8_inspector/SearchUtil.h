// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SearchUtil_h
#define SearchUtil_h

#include "platform/v8_inspector/StringUtil.h"
#include "platform/v8_inspector/protocol/Debugger.h"

namespace v8_inspector {

class V8InspectorSession;

String16 findSourceURL(const String16& content, bool multiline);
String16 findSourceMapURL(const String16& content, bool multiline);
std::vector<std::unique_ptr<protocol::Debugger::SearchMatch>> searchInTextByLinesImpl(V8InspectorSession*, const String16& text, const String16& query, bool caseSensitive, bool isRegex);

} //  namespace v8_inspector

#endif // !defined(SearchUtil_h)
