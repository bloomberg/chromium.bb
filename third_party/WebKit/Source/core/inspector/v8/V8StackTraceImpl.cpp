// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/inspector/v8/V8StackTraceImpl.h"

#include "core/inspector/v8/V8DebuggerAgentImpl.h"
#include "core/inspector/v8/V8DebuggerImpl.h"
#include "core/inspector/v8/V8StringUtil.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/text/StringBuilder.h"

#include <v8-debug.h>
#include <v8-profiler.h>

namespace blink {

namespace {

V8StackTraceImpl::Frame toCallFrame(v8::Local<v8::StackFrame> frame)
{
    String scriptId = String::number(frame->GetScriptId());
    String sourceName;
    v8::Local<v8::String> sourceNameValue(frame->GetScriptNameOrSourceURL());
    if (!sourceNameValue.IsEmpty())
        sourceName = toWTFString(sourceNameValue);

    String functionName;
    v8::Local<v8::String> functionNameValue(frame->GetFunctionName());
    if (!functionNameValue.IsEmpty())
        functionName = toWTFString(functionNameValue);

    int sourceLineNumber = frame->GetLineNumber();
    int sourceColumn = frame->GetColumn();
    return V8StackTraceImpl::Frame(functionName, scriptId, sourceName, sourceLineNumber, sourceColumn);
}

void toCallFramesVector(v8::Local<v8::StackTrace> stackTrace, Vector<V8StackTraceImpl::Frame>& scriptCallFrames, size_t maxStackSize, v8::Isolate* isolate)
{
    ASSERT(isolate->InContext());
    int frameCount = stackTrace->GetFrameCount();
    if (frameCount > static_cast<int>(maxStackSize))
        frameCount = maxStackSize;
    for (int i = 0; i < frameCount; i++) {
        v8::Local<v8::StackFrame> stackFrame = stackTrace->GetFrame(i);
        scriptCallFrames.append(toCallFrame(stackFrame));
    }
}

} //  namespace

V8StackTraceImpl::Frame::Frame()
    : m_functionName("undefined")
    , m_scriptId("")
    , m_scriptName("undefined")
    , m_lineNumber(0)
    , m_columnNumber(0)
{
}

V8StackTraceImpl::Frame::Frame(const String& functionName, const String& scriptId, const String& scriptName, int lineNumber, int column)
    : m_functionName(functionName)
    , m_scriptId(scriptId)
    , m_scriptName(scriptName)
    , m_lineNumber(lineNumber)
    , m_columnNumber(column)
{
}

V8StackTraceImpl::Frame::~Frame()
{
}

// buildInspectorObject() and ScriptCallStack's toTracedValue() should set the same fields.
// If either of them is modified, the other should be also modified.
PassRefPtr<protocol::TypeBuilder::Runtime::CallFrame> V8StackTraceImpl::Frame::buildInspectorObject() const
{
    return protocol::TypeBuilder::Runtime::CallFrame::create()
        .setFunctionName(m_functionName)
        .setScriptId(m_scriptId)
        .setUrl(m_scriptName)
        .setLineNumber(m_lineNumber)
        .setColumnNumber(m_columnNumber)
        .release();
}

PassOwnPtr<V8StackTraceImpl> V8StackTraceImpl::create(V8DebuggerAgentImpl* agent, v8::Local<v8::StackTrace> stackTrace, size_t maxStackSize)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    ASSERT(isolate->InContext());
    ASSERT(!stackTrace.IsEmpty());

    v8::HandleScope scope(isolate);
    Vector<V8StackTraceImpl::Frame> scriptCallFrames;
    toCallFramesVector(stackTrace, scriptCallFrames, maxStackSize, isolate);

    OwnPtr<V8StackTraceImpl> callStack;
    if (agent && maxStackSize > 1)
        callStack = agent->currentAsyncStackTraceForRuntime();
    return V8StackTraceImpl::create(String(), scriptCallFrames, callStack.release());
}

PassOwnPtr<V8StackTraceImpl> V8StackTraceImpl::capture(V8DebuggerAgentImpl* agent, size_t maxStackSize)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    if (!isolate->InContext())
        return nullptr;
    v8::HandleScope handleScope(isolate);
    isolate->GetCpuProfiler()->CollectSample();
    v8::Local<v8::StackTrace> stackTrace(v8::StackTrace::CurrentStackTrace(isolate, maxStackSize, stackTraceOptions));
    return V8StackTraceImpl::create(agent, stackTrace, maxStackSize);
}

PassOwnPtr<V8StackTraceImpl> V8StackTraceImpl::create(const String& description, Vector<Frame>& frames, PassOwnPtr<V8StackTraceImpl> parent)
{
    return adoptPtr(new V8StackTraceImpl(description, frames, parent));
}

V8StackTraceImpl::V8StackTraceImpl(const String& description, Vector<Frame>& frames, PassOwnPtr<V8StackTraceImpl> parent)
    : m_description(description)
    , m_parent(parent)
{
    m_frames.swap(frames);
}

V8StackTraceImpl::~V8StackTraceImpl()
{
}

String V8StackTraceImpl::topSourceURL() const
{
    ASSERT(m_frames.size());
    return m_frames[0].m_scriptName;
}

int V8StackTraceImpl::topLineNumber() const
{
    ASSERT(m_frames.size());
    return m_frames[0].m_lineNumber;
}

int V8StackTraceImpl::topColumnNumber() const
{
    ASSERT(m_frames.size());
    return m_frames[0].m_columnNumber;
}

String V8StackTraceImpl::topFunctionName() const
{
    ASSERT(m_frames.size());
    return m_frames[0].m_functionName;
}

String V8StackTraceImpl::topScriptId() const
{
    ASSERT(m_frames.size());
    return m_frames[0].m_scriptId;
}

PassRefPtr<protocol::TypeBuilder::Runtime::StackTrace> V8StackTraceImpl::buildInspectorObject() const
{
    RefPtr<protocol::TypeBuilder::Array<protocol::TypeBuilder::Runtime::CallFrame>> frames = protocol::TypeBuilder::Array<protocol::TypeBuilder::Runtime::CallFrame>::create();
    for (size_t i = 0; i < m_frames.size(); i++)
        frames->addItem(m_frames.at(i).buildInspectorObject());

    RefPtr<protocol::TypeBuilder::Runtime::StackTrace> stackTrace = protocol::TypeBuilder::Runtime::StackTrace::create()
        .setCallFrames(frames.release());
    if (!m_description.isEmpty())
        stackTrace->setDescription(m_description);
    if (m_parent)
        stackTrace->setParent(m_parent->buildInspectorObject());
    return stackTrace;
}

String V8StackTraceImpl::toString() const
{
    StringBuilder stackTrace;
    for (size_t i = 0; i < m_frames.size(); ++i) {
        const Frame& frame = m_frames[i];
        stackTrace.append("\n    at " + (frame.functionName().length() ? frame.functionName() : "(anonymous function)"));
        stackTrace.appendLiteral(" (");
        stackTrace.append(frame.sourceURL());
        stackTrace.append(':');
        stackTrace.appendNumber(frame.lineNumber());
        stackTrace.append(':');
        stackTrace.appendNumber(frame.columnNumber());
        stackTrace.append(')');
    }
    return stackTrace.toString();
}

} // namespace blink
