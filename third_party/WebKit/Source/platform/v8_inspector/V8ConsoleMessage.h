// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8ConsoleMessage_h
#define V8ConsoleMessage_h

#include "platform/inspector_protocol/Collections.h"
#include "platform/inspector_protocol/String16.h"
#include "platform/v8_inspector/protocol/Console.h"
#include "platform/v8_inspector/protocol/Runtime.h"
#include "platform/v8_inspector/public/V8ConsoleTypes.h"
#include "platform/v8_inspector/public/V8StackTrace.h"
#include <deque>
#include <v8.h>

namespace blink {

class InspectedContext;
class V8DebuggerImpl;
class V8InspectorSessionImpl;
class V8StackTrace;

enum class V8MessageOrigin { kConsole, kException, kRevokedException };

class V8ConsoleMessage {
public:
    V8ConsoleMessage(
        double timestamp,
        MessageSource,
        MessageLevel,
        const String16& message,
        const String16& url,
        unsigned lineNumber,
        unsigned columnNumber,
        std::unique_ptr<V8StackTrace>,
        int scriptId,
        const String16& requestIdentifier);
    ~V8ConsoleMessage();

    static std::unique_ptr<V8ConsoleMessage> createForConsoleAPI(
        double timestamp,
        MessageType,
        MessageLevel,
        const String16& message,
        std::vector<v8::Local<v8::Value>>* arguments,
        std::unique_ptr<V8StackTrace>,
        InspectedContext*);

    static std::unique_ptr<V8ConsoleMessage> createForException(
        double timestamp,
        const String16& message,
        const String16& url,
        unsigned lineNumber,
        unsigned columnNumber,
        std::unique_ptr<V8StackTrace>,
        int scriptId,
        v8::Isolate*,
        int contextId,
        v8::Local<v8::Value> exception,
        unsigned exceptionId);

    static std::unique_ptr<V8ConsoleMessage> createForRevokedException(
        double timestamp,
        const String16& message,
        unsigned revokedExceptionId);

    V8MessageOrigin origin() const;
    void reportToFrontend(protocol::Console::Frontend*, V8InspectorSessionImpl*, bool generatePreview) const;
    void reportToFrontend(protocol::Runtime::Frontend*, V8InspectorSessionImpl*, bool generatePreview) const;
    unsigned argumentCount() const;
    MessageType type() const;
    void contextDestroyed(int contextId);

private:
    using Arguments = std::vector<std::unique_ptr<v8::Global<v8::Value>>>;
    std::unique_ptr<protocol::Array<protocol::Runtime::RemoteObject>> wrapArguments(V8InspectorSessionImpl*, bool generatePreview) const;
    std::unique_ptr<protocol::Runtime::RemoteObject> wrapException(V8InspectorSessionImpl*, bool generatePreview) const;

    V8MessageOrigin m_origin;
    double m_timestamp;
    MessageSource m_source;
    MessageLevel m_level;
    String16 m_message;
    String16 m_url;
    unsigned m_lineNumber;
    unsigned m_columnNumber;
    std::unique_ptr<V8StackTrace> m_stackTrace;
    int m_scriptId;
    String16 m_requestIdentifier;
    int m_contextId;
    MessageType m_type;
    unsigned m_exceptionId;
    unsigned m_revokedExceptionId;
    Arguments m_arguments;
};

class V8ConsoleMessageStorage {
public:
    V8ConsoleMessageStorage(V8DebuggerImpl*, int contextGroupId);
    ~V8ConsoleMessageStorage();

    int contextGroupId() { return m_contextGroupId; }
    int expiredCount() { return m_expiredCount; }
    const std::deque<std::unique_ptr<V8ConsoleMessage>>& messages() const { return m_messages; }

    void addMessage(std::unique_ptr<V8ConsoleMessage>);
    void contextDestroyed(int contextId);
    void clear();

private:
    V8DebuggerImpl* m_debugger;
    int m_contextGroupId;
    int m_expiredCount;
    std::deque<std::unique_ptr<V8ConsoleMessage>> m_messages;
};

} // namespace blink

#endif // V8ConsoleMessage_h
