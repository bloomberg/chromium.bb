// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ConsoleMessage_h
#define ConsoleMessage_h

#include "bindings/core/v8/ScriptState.h"
#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "platform/v8_inspector/public/ConsoleAPITypes.h"
#include "platform/v8_inspector/public/ConsoleTypes.h"
#include "wtf/Forward.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ScriptArguments;
class ScriptCallStack;
class ScriptState;
class SourceLocation;
class V8StackTrace;

class CORE_EXPORT ConsoleMessage final: public GarbageCollectedFinalized<ConsoleMessage> {
public:
    // Callstack may be empty. Zero lineNumber or columnNumber means unknown.
    static ConsoleMessage* create(MessageSource, MessageLevel, const String& message, const String& url, unsigned lineNumber, unsigned columnNumber, PassRefPtr<ScriptCallStack>, int scriptId = 0, ScriptArguments* = nullptr);

    // Shortcut when callstack is unavailable.
    static ConsoleMessage* create(MessageSource, MessageLevel, const String& message, const String& url, unsigned lineNumber, unsigned columnNumber);

    // This method tries to capture callstack if possible and falls back to provided location.
    static ConsoleMessage* createWithCallStack(MessageSource, MessageLevel, const String& message, const String& url, unsigned lineNumber, unsigned columnNumber);

    // Shortcut when location is unavailable. This method captures callstack.
    static ConsoleMessage* create(MessageSource, MessageLevel, const String& message);

    // This method captures callstack.
    static ConsoleMessage* createForRequest(MessageSource, MessageLevel, const String& message, const String& url, unsigned long requestIdentifier);

    // This method captures callstack.
    static ConsoleMessage* createForConsoleAPI(MessageLevel, MessageType, const String& message, ScriptArguments*);

    static ConsoleMessage* create(MessageSource, MessageLevel, const String& message, PassOwnPtr<SourceLocation>);
    static ConsoleMessage* create(MessageSource, MessageLevel, const String& message, const String& url, unsigned lineNumber, unsigned columnNumber, PassOwnPtr<V8StackTrace>, int scriptId, ScriptArguments*);

    ~ConsoleMessage();

    MessageType type() const;
    int scriptId() const;
    const String& url() const;
    unsigned lineNumber() const;
    unsigned columnNumber() const;
    V8StackTrace* stackTrace() const;
    ScriptArguments* scriptArguments() const;
    unsigned long requestIdentifier() const;
    double timestamp() const;
    unsigned assignMessageId();
    unsigned messageId() const { return m_messageId; }
    unsigned relatedMessageId() const { return m_relatedMessageId; }
    void setRelatedMessageId(unsigned relatedMessageId) { m_relatedMessageId = relatedMessageId; }
    MessageSource source() const;
    MessageLevel level() const;
    const String& message() const;

    void frameWindowDiscarded(LocalDOMWindow*);
    unsigned argumentCount();

    DECLARE_TRACE();

private:
    ConsoleMessage(MessageSource, MessageLevel, const String& message, const String& url, unsigned lineNumber, unsigned columnNumber, PassOwnPtr<V8StackTrace>, int scriptId, ScriptArguments*);

    MessageSource m_source;
    MessageLevel m_level;
    MessageType m_type;
    String m_message;
    int m_scriptId;
    String m_url;
    unsigned m_lineNumber;
    unsigned m_columnNumber;
    OwnPtr<V8StackTrace> m_stackTrace;
    Member<ScriptArguments> m_scriptArguments;
    unsigned long m_requestIdentifier;
    double m_timestamp;
    unsigned m_messageId;
    unsigned m_relatedMessageId;
};

} // namespace blink

#endif // ConsoleMessage_h
