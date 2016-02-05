// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/inspector/v8/V8Regex.h"

#include "core/inspector/v8/V8DebuggerClient.h"
#include "core/inspector/v8/V8DebuggerImpl.h"
#include "core/inspector/v8/V8StringUtil.h"

namespace blink {

V8Regex::V8Regex(V8DebuggerImpl* debugger, const String& pattern, TextCaseSensitivity caseSensitivity, MultilineMode multilineMode)
    : m_debugger(debugger)
{
    v8::Isolate* isolate = m_debugger->isolate();
    v8::HandleScope handleScope(isolate);
    v8::Local<v8::Context> context = m_debugger->regexContext();
    v8::Context::Scope contextScope(context);
    v8::TryCatch tryCatch(isolate);

    unsigned flags = v8::RegExp::kNone;
    if (caseSensitivity == TextCaseInsensitive)
        flags |= v8::RegExp::kIgnoreCase;
    if (multilineMode == MultilineEnabled)
        flags |= v8::RegExp::kMultiline;

    v8::Local<v8::RegExp> regex;
    if (v8::RegExp::New(context, toV8String(isolate, pattern), static_cast<v8::RegExp::Flags>(flags)).ToLocal(&regex))
        m_regex.Reset(isolate, regex);
}

int V8Regex::match(const String& string, int startFrom, int* matchLength) const
{
    if (matchLength)
        *matchLength = 0;

    if (m_regex.IsEmpty() || string.isNull())
        return -1;

    // v8 strings are limited to int.
    if (string.length() > INT_MAX)
        return -1;

    v8::Isolate* isolate = m_debugger->isolate();
    v8::HandleScope handleScope(isolate);
    v8::Local<v8::Context> context = m_debugger->regexContext();
    v8::Context::Scope contextScope(context);
    v8::TryCatch tryCatch(isolate);

    v8::Local<v8::RegExp> regex = m_regex.Get(isolate);
    v8::Local<v8::Value> exec;
    if (!regex->Get(context, toV8StringInternalized(isolate, "exec")).ToLocal(&exec))
        return -1;
    v8::Local<v8::Value> argv[] = { toV8String(isolate, string.substring(startFrom)) };
    v8::Local<v8::Value> returnValue;
    if (!m_debugger->client()->callInternalFunction(exec.As<v8::Function>(), regex, WTF_ARRAY_LENGTH(argv), argv).ToLocal(&returnValue))
        return -1;

    // RegExp#exec returns null if there's no match, otherwise it returns an
    // Array of strings with the first being the whole match string and others
    // being subgroups. The Array also has some random properties tacked on like
    // "index" which is the offset of the match.
    //
    // https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/RegExp/exec

    ASSERT(!returnValue.IsEmpty());
    if (!returnValue->IsArray())
        return -1;

    v8::Local<v8::Array> result = returnValue.As<v8::Array>();
    v8::Local<v8::Value> matchOffset;
    if (!result->Get(context, toV8StringInternalized(isolate, "index")).ToLocal(&matchOffset))
        return -1;
    if (matchLength) {
        v8::Local<v8::Value> match;
        if (!result->Get(context, 0).ToLocal(&match))
            return -1;
        *matchLength = match.As<v8::String>()->Length();
    }

    return matchOffset.As<v8::Int32>()->Value() + startFrom;
}

} // namespace blink
