// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8StackTraceImpl_h
#define V8StackTraceImpl_h

#include "core/inspector/v8/public/V8StackTrace.h"
#include "wtf/Forward.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/Vector.h"

namespace blink {

class TracedValue;
class V8DebuggerAgentImpl;

class V8StackTraceImpl final : public V8StackTrace {
    WTF_MAKE_NONCOPYABLE(V8StackTraceImpl);
    USING_FAST_MALLOC(V8StackTraceImpl);
public:
    class Frame  {
        DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
    public:
        Frame();
        Frame(const String& functionName, const String& scriptId, const String& scriptName, int lineNumber, int column = 0);
        ~Frame();

        const String& functionName() const { return m_functionName; }
        const String& scriptId() const { return m_scriptId; }
        const String& sourceURL() const { return m_scriptName; }
        int lineNumber() const { return m_lineNumber; }
        int columnNumber() const { return m_columnNumber; }

    private:
        friend class V8StackTraceImpl;
        PassRefPtr<protocol::TypeBuilder::Runtime::CallFrame> buildInspectorObject() const;
        void toTracedValue(TracedValue*) const;

        String m_functionName;
        String m_scriptId;
        String m_scriptName;
        int m_lineNumber;
        int m_columnNumber;
    };

    static PassOwnPtr<V8StackTraceImpl> create(const String& description, Vector<Frame>&, PassOwnPtr<V8StackTraceImpl>);
    static PassOwnPtr<V8StackTraceImpl> create(V8DebuggerAgentImpl*, v8::Local<v8::StackTrace>, size_t maxStackSize);
    static PassOwnPtr<V8StackTraceImpl> capture(V8DebuggerAgentImpl*, size_t maxStackSize);

    ~V8StackTraceImpl() override;

    // V8StackTrace implementation.
    bool isEmpty() const override { return !m_frames.size(); };
    String topSourceURL() const override;
    int topLineNumber() const override;
    int topColumnNumber() const override;
    String topScriptId() const override;
    String topFunctionName() const override;
    PassRefPtr<protocol::TypeBuilder::Runtime::StackTrace> buildInspectorObject() const override;
    String toString() const override;

private:
    V8StackTraceImpl(const String& description, Vector<Frame>& frames, PassOwnPtr<V8StackTraceImpl> parent);

    String m_description;
    Vector<Frame> m_frames;
    OwnPtr<V8StackTraceImpl> m_parent;
};

} // namespace blink

#endif // V8StackTraceImpl_h
