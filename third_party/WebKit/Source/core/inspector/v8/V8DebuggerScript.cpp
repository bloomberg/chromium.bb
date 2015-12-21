// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/inspector/v8/V8DebuggerScript.h"

namespace {
static const unsigned kBlackboxUnknown = 0;
}

namespace blink {

V8DebuggerScript::V8DebuggerScript()
    : m_startLine(0)
    , m_startColumn(0)
    , m_endLine(0)
    , m_endColumn(0)
    , m_executionContextId(0)
    , m_isContentScript(false)
    , m_isInternalScript(false)
    , m_isLiveEdit(false)
    , m_isBlackboxedURL(false)
    , m_blackboxGeneration(kBlackboxUnknown)
{
}

String V8DebuggerScript::sourceURL() const
{
    return m_sourceURL.isEmpty() ? m_url : m_sourceURL;
}

bool V8DebuggerScript::getBlackboxedState(unsigned blackboxGeneration, bool* isBlackboxed) const
{
    if (m_blackboxGeneration == kBlackboxUnknown || m_blackboxGeneration != blackboxGeneration)
        return false;
    *isBlackboxed = m_isBlackboxedURL;
    return true;
}

void V8DebuggerScript::setBlackboxedState(unsigned blackboxGeneration, bool isBlackboxed)
{
    ASSERT(blackboxGeneration);
    m_isBlackboxedURL = isBlackboxed;
    m_blackboxGeneration = blackboxGeneration;
}

V8DebuggerScript& V8DebuggerScript::setURL(const String& url)
{
    m_url = url;
    m_blackboxGeneration = kBlackboxUnknown;
    return *this;
}

V8DebuggerScript& V8DebuggerScript::setSourceURL(const String& sourceURL)
{
    m_sourceURL = sourceURL;
    m_blackboxGeneration = kBlackboxUnknown;
    return *this;
}

V8DebuggerScript& V8DebuggerScript::setSourceMappingURL(const String& sourceMappingURL)
{
    m_sourceMappingURL = sourceMappingURL;
    return *this;
}

V8DebuggerScript& V8DebuggerScript::setSource(const String& source)
{
    m_source = source;
    return *this;
}

V8DebuggerScript& V8DebuggerScript::setStartLine(int startLine)
{
    m_startLine = startLine;
    return *this;
}

V8DebuggerScript& V8DebuggerScript::setStartColumn(int startColumn)
{
    m_startColumn = startColumn;
    return *this;
}

V8DebuggerScript& V8DebuggerScript::setEndLine(int endLine)
{
    m_endLine = endLine;
    return *this;
}

V8DebuggerScript& V8DebuggerScript::setEndColumn(int endColumn)
{
    m_endColumn = endColumn;
    return *this;
}

V8DebuggerScript& V8DebuggerScript::setExecutionContextId(int executionContextId)
{
    m_executionContextId = executionContextId;
    return *this;
}

V8DebuggerScript& V8DebuggerScript::setIsContentScript(bool isContentScript)
{
    m_isContentScript = isContentScript;
    return *this;
}

V8DebuggerScript& V8DebuggerScript::setIsInternalScript(bool isInternalScript)
{
    m_isInternalScript = isInternalScript;
    return *this;
}

V8DebuggerScript& V8DebuggerScript::setIsLiveEdit(bool isLiveEdit)
{
    m_isLiveEdit = isLiveEdit;
    return *this;
}

} // namespace blink
