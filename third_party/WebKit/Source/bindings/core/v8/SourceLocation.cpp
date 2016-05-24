// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/SourceLocation.h"

#include "bindings/core/v8/V8PerIsolateData.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/ScriptableDocumentParser.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/ThreadDebugger.h"
#include "platform/ScriptForbiddenScope.h"
#include "platform/TracedValue.h"
#include "platform/v8_inspector/public/V8Debugger.h"

namespace blink {

namespace {

PassOwnPtr<V8StackTrace> captureStackTrace()
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    V8PerIsolateData* data = V8PerIsolateData::from(isolate);
    if (!data->threadDebugger() || !isolate->InContext())
        return nullptr;
    size_t stackSize = 1;
    if (InspectorInstrumentation::hasFrontends()) {
        if (InspectorInstrumentation::consoleAgentEnabled(currentExecutionContext(isolate)))
            stackSize = V8StackTrace::maxCallStackSizeToCapture;
    }
    ScriptForbiddenScope::AllowUserAgentScript allowScripting;
    return data->threadDebugger()->debugger()->captureStackTrace(stackSize);
}

}

// static
PassOwnPtr<SourceLocation> SourceLocation::capture(const String& url, unsigned lineNumber, unsigned columnNumber)
{
    OwnPtr<V8StackTrace> stackTrace = captureStackTrace();
    if (stackTrace && !stackTrace->isEmpty())
        return SourceLocation::create(stackTrace->topSourceURL(), stackTrace->topLineNumber(), stackTrace->topColumnNumber(), std::move(stackTrace), 0);
    return SourceLocation::create(url, lineNumber, columnNumber, std::move(stackTrace));
}

// static
PassOwnPtr<SourceLocation> SourceLocation::capture(ExecutionContext* executionContext)
{
    OwnPtr<V8StackTrace> stackTrace = captureStackTrace();
    if (stackTrace && !stackTrace->isEmpty())
        return SourceLocation::create(stackTrace->topSourceURL(), stackTrace->topLineNumber(), stackTrace->topColumnNumber(), std::move(stackTrace), 0);

    Document* document = executionContext && executionContext->isDocument() ? toDocument(executionContext) : nullptr;
    if (document) {
        unsigned lineNumber = 0;
        if (document->scriptableDocumentParser() && !document->isInDocumentWrite()) {
            if (document->scriptableDocumentParser()->isParsingAtLineNumber())
                lineNumber = document->scriptableDocumentParser()->lineNumber().oneBasedInt();
        }
        return SourceLocation::create(document->url().getString(), lineNumber, 0, std::move(stackTrace));
    }

    return SourceLocation::create(executionContext ? executionContext->url().getString() : "", 0, 0, std::move(stackTrace));
}

// static
PassOwnPtr<SourceLocation> SourceLocation::create(const String& url, unsigned lineNumber, unsigned columnNumber, PassOwnPtr<V8StackTrace> stackTrace, int scriptId)
{
    return adoptPtr(new SourceLocation(url, lineNumber, columnNumber, std::move(stackTrace), scriptId));
}

SourceLocation::SourceLocation(const String& url, unsigned lineNumber, unsigned columnNumber, PassOwnPtr<V8StackTrace> stackTrace, int scriptId)
    : m_url(url)
    , m_lineNumber(lineNumber)
    , m_columnNumber(columnNumber)
    , m_stackTrace(std::move(stackTrace))
    , m_scriptId(scriptId)
{
}

SourceLocation::~SourceLocation()
{
}

} // namespace blink
