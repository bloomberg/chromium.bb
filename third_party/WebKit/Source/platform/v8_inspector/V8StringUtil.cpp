// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/v8_inspector/V8StringUtil.h"

#include "platform/v8_inspector/V8DebuggerImpl.h"
#include "platform/v8_inspector/V8Regex.h"
#include "platform/v8_inspector/public/V8ContentSearchUtil.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/TextPosition.h"

namespace blink {

namespace {

String findMagicComment(const String& content, const String& name, bool multiline)
{
    ASSERT(name.find("=") == kNotFound);

    unsigned length = content.length();
    unsigned nameLength = name.length();

    size_t pos = length;
    size_t equalSignPos = 0;
    size_t closingCommentPos = 0;
    while (true) {
        pos = content.reverseFind(name, pos);
        if (pos == kNotFound)
            return String();

        // Check for a /\/[\/*][@#][ \t]/ regexp (length of 4) before found name.
        if (pos < 4)
            return String();
        pos -= 4;
        if (content[pos] != '/')
            continue;
        if ((content[pos + 1] != '/' || multiline)
            && (content[pos + 1] != '*' || !multiline))
            continue;
        if (content[pos + 2] != '#')
            continue;
        if (content[pos + 3] != ' ' && content[pos + 3] != '\t')
            continue;
        equalSignPos = pos + 4 + nameLength;
        if (equalSignPos < length && content[equalSignPos] != '=')
            continue;
        if (multiline) {
            closingCommentPos = content.find("*/", equalSignPos + 1);
            if (closingCommentPos == kNotFound)
                return String();
        }

        break;
    }

    ASSERT(equalSignPos);
    ASSERT(!multiline || closingCommentPos);
    size_t urlPos = equalSignPos + 1;
    String match = multiline
        ? content.substring(urlPos, closingCommentPos - urlPos)
        : content.substring(urlPos);

    size_t newLine = match.find("\n");
    if (newLine != kNotFound)
        match = match.substring(0, newLine);
    match = match.stripWhiteSpace();

    String disallowedChars("\"' \t");
    for (unsigned i = 0; i < match.length(); ++i) {
        if (disallowedChars.find(match[i]) != kNotFound)
            return "";
    }

    return match;
}

String createSearchRegexSource(const String& text)
{
    StringBuilder result;
    String specials("[](){}+-*.,?\\^$|");

    for (unsigned i = 0; i < text.length(); i++) {
        if (specials.find(text[i]) != kNotFound)
            result.append('\\');
        result.append(text[i]);
    }

    return result.toString();
}

Vector<std::pair<int, String>> scriptRegexpMatchesByLines(const V8Regex& regex, const String& text)
{
    Vector<std::pair<int, String>> result;
    if (text.isEmpty())
        return result;

    OwnPtr<Vector<unsigned>> endings(lineEndings(text));
    unsigned size = endings->size();
    unsigned start = 0;
    for (unsigned lineNumber = 0; lineNumber < size; ++lineNumber) {
        unsigned lineEnd = endings->at(lineNumber);
        String line = text.substring(start, lineEnd - start);
        if (line.endsWith('\r'))
            line = line.left(line.length() - 1);

        int matchLength;
        if (regex.match(line, 0, &matchLength) != -1)
            result.append(std::pair<int, String>(lineNumber, line));

        start = lineEnd + 1;
    }
    return result;
}

PassRefPtr<protocol::TypeBuilder::Debugger::SearchMatch> buildObjectForSearchMatch(int lineNumber, const String& lineContent)
{
    return protocol::TypeBuilder::Debugger::SearchMatch::create()
        .setLineNumber(lineNumber)
        .setLineContent(lineContent)
        .release();
}

PassOwnPtr<V8Regex> createSearchRegex(V8DebuggerImpl* debugger, const String& query, bool caseSensitive, bool isRegex)
{
    String regexSource = isRegex ? query : createSearchRegexSource(query);
    return adoptPtr(new V8Regex(debugger, regexSource, caseSensitive ? TextCaseSensitive : TextCaseInsensitive));
}

} // namespace

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

namespace V8ContentSearchUtil {

String findSourceURL(const String& content, bool multiline)
{
    return findMagicComment(content, "sourceURL", multiline);
}

String findSourceMapURL(const String& content, bool multiline)
{
    return findMagicComment(content, "sourceMappingURL", multiline);
}

PassRefPtr<protocol::TypeBuilder::Array<protocol::TypeBuilder::Debugger::SearchMatch>> searchInTextByLines(V8Debugger* debugger, const String& text, const String& query, const bool caseSensitive, const bool isRegex)
{
    RefPtr<protocol::TypeBuilder::Array<protocol::TypeBuilder::Debugger::SearchMatch>> result = protocol::TypeBuilder::Array<protocol::TypeBuilder::Debugger::SearchMatch>::create();
    OwnPtr<V8Regex> regex = createSearchRegex(static_cast<V8DebuggerImpl*>(debugger), query, caseSensitive, isRegex);
    Vector<std::pair<int, String>> matches = scriptRegexpMatchesByLines(*regex.get(), text);

    for (const auto& match : matches)
        result->addItem(buildObjectForSearchMatch(match.first, match.second));

    return result;
}

} // namespace V8ContentSearchUtil

} // namespace blink
