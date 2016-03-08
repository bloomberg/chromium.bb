// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/v8_inspector/V8StackTraceImpl.h"

#include "platform/inspector_protocol/String16.h"
#include "platform/v8_inspector/V8DebuggerAgentImpl.h"
#include "platform/v8_inspector/V8DebuggerImpl.h"
#include "platform/v8_inspector/V8StringUtil.h"
#include "wtf/PassOwnPtr.h"

#include <v8-debug.h>
#include <v8-profiler.h>

namespace blink {

namespace {

V8StackTraceImpl::Frame toCallFrame(v8::Local<v8::StackFrame> frame)
{
    String16 scriptId = String16::number(frame->GetScriptId());
    String16 sourceName;
    v8::Local<v8::String> sourceNameValue(frame->GetScriptNameOrSourceURL());
    if (!sourceNameValue.IsEmpty())
        sourceName = toProtocolString(sourceNameValue);

    String16 functionName;
    v8::Local<v8::String> functionNameValue(frame->GetFunctionName());
    if (!functionNameValue.IsEmpty())
        functionName = toProtocolString(functionNameValue);

    int sourceLineNumber = frame->GetLineNumber();
    int sourceColumn = frame->GetColumn();
    return V8StackTraceImpl::Frame(functionName, scriptId, sourceName, sourceLineNumber, sourceColumn);
}

void toCallFramesVector(v8::Local<v8::StackTrace> stackTrace, protocol::Vector<V8StackTraceImpl::Frame>& scriptCallFrames, size_t maxStackSize, v8::Isolate* isolate)
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

V8StackTraceImpl::Frame::Frame(const String16& functionName, const String16& scriptId, const String16& scriptName, int lineNumber, int column)
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
PassOwnPtr<protocol::Runtime::CallFrame> V8StackTraceImpl::Frame::buildInspectorObject() const
{
    return protocol::Runtime::CallFrame::create()
        .setFunctionName(m_functionName)
        .setScriptId(m_scriptId)
        .setUrl(m_scriptName)
        .setLineNumber(m_lineNumber)
        .setColumnNumber(m_columnNumber)
        .build();
}

PassOwnPtr<V8StackTraceImpl> V8StackTraceImpl::create(V8DebuggerAgentImpl* agent, v8::Local<v8::StackTrace> stackTrace, size_t maxStackSize, const String16& description)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(isolate);
    protocol::Vector<V8StackTraceImpl::Frame> scriptCallFrames;
    if (!stackTrace.IsEmpty())
        toCallFramesVector(stackTrace, scriptCallFrames, maxStackSize, isolate);

    OwnPtr<V8StackTraceImpl> asyncCallStack;
    if (agent && agent->trackingAsyncCalls() && maxStackSize > 1)
        asyncCallStack = agent->currentAsyncStackTraceForRuntime();

    if (stackTrace.IsEmpty() && !asyncCallStack)
        return nullptr;

    return V8StackTraceImpl::create(description, scriptCallFrames, asyncCallStack.release());
}

PassOwnPtr<V8StackTraceImpl> V8StackTraceImpl::capture(V8DebuggerAgentImpl* agent, size_t maxStackSize, const String16& description)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope handleScope(isolate);
    v8::Local<v8::StackTrace> stackTrace;
    if (isolate->InContext()) {
        isolate->GetCpuProfiler()->CollectSample();
        stackTrace = v8::StackTrace::CurrentStackTrace(isolate, maxStackSize, stackTraceOptions);
    }
    return V8StackTraceImpl::create(agent, stackTrace, maxStackSize, description);
}

PassOwnPtr<V8StackTraceImpl> V8StackTraceImpl::create(const String16& description, protocol::Vector<Frame>& frames, PassOwnPtr<V8StackTraceImpl> parent)
{
    return adoptPtr(new V8StackTraceImpl(description, frames, parent));
}

PassOwnPtr<V8StackTraceImpl> V8StackTraceImpl::clone(V8StackTraceImpl* origin, size_t maxStackSize)
{
    if (!origin)
        return nullptr;

    // TODO(dgozman): move this check to call-site.
    if (!origin->m_frames.size())
        return V8StackTraceImpl::clone(origin->m_parent.get(), maxStackSize);

    OwnPtr<V8StackTraceImpl> parent;
    if (origin->m_parent && maxStackSize)
        parent = V8StackTraceImpl::clone(origin->m_parent.get(), maxStackSize - 1);

    protocol::Vector<Frame> frames(origin->m_frames);
    return adoptPtr(new V8StackTraceImpl(origin->m_description, frames, parent.release()));
}

V8StackTraceImpl::V8StackTraceImpl(const String16& description, protocol::Vector<Frame>& frames, PassOwnPtr<V8StackTraceImpl> parent)
    : m_description(description)
    , m_parent(parent)
{
    m_frames.swap(frames);
}

V8StackTraceImpl::~V8StackTraceImpl()
{
}

String16 V8StackTraceImpl::topSourceURL() const
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

String16 V8StackTraceImpl::topFunctionName() const
{
    ASSERT(m_frames.size());
    return m_frames[0].m_functionName;
}

String16 V8StackTraceImpl::topScriptId() const
{
    ASSERT(m_frames.size());
    return m_frames[0].m_scriptId;
}

PassOwnPtr<protocol::Runtime::StackTrace> V8StackTraceImpl::buildInspectorObject() const
{
    OwnPtr<protocol::Array<protocol::Runtime::CallFrame>> frames = protocol::Array<protocol::Runtime::CallFrame>::create();
    for (size_t i = 0; i < m_frames.size(); i++)
        frames->addItem(m_frames.at(i).buildInspectorObject());

    OwnPtr<protocol::Runtime::StackTrace> stackTrace = protocol::Runtime::StackTrace::create()
        .setCallFrames(frames.release()).build();
    if (!m_description.isEmpty())
        stackTrace->setDescription(m_description);
    if (m_parent)
        stackTrace->setParent(m_parent->buildInspectorObject());
    return stackTrace.release();
}

String16 V8StackTraceImpl::toString() const
{
    String16Builder stackTrace;
    for (size_t i = 0; i < m_frames.size(); ++i) {
        const Frame& frame = m_frames[i];
        stackTrace.append("\n    at " + (frame.functionName().length() ? frame.functionName() : "(anonymous function)"));
        stackTrace.append(" (");
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
