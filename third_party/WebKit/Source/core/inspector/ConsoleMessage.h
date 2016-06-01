// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ConsoleMessage_h
#define ConsoleMessage_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "platform/v8_inspector/public/ConsoleAPITypes.h"
#include "platform/v8_inspector/public/ConsoleTypes.h"
#include "wtf/Forward.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/text/WTFString.h"

namespace blink {

class LocalDOMWindow;
class ScriptArguments;
class SourceLocation;
class V8StackTrace;

class CORE_EXPORT ConsoleMessage final: public GarbageCollectedFinalized<ConsoleMessage> {
public:
    // Location should not be null. Zero lineNumber or columnNumber means unknown.
    static ConsoleMessage* create(MessageSource, MessageLevel, const String& message, PassOwnPtr<SourceLocation>, ScriptArguments* = nullptr);

    // Shortcut when location is unknown. Captures current location.
    static ConsoleMessage* create(MessageSource, MessageLevel, const String& message);

    // This method captures current location.
    static ConsoleMessage* createForRequest(MessageSource, MessageLevel, const String& message, const String& url, unsigned long requestIdentifier);

    // This method captures current location.
    static ConsoleMessage* createForConsoleAPI(MessageLevel, MessageType, const String& message, ScriptArguments*);

    ~ConsoleMessage();

    MessageType type() const;
    SourceLocation* location() const;
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
    ConsoleMessage(MessageSource, MessageLevel, const String& message, PassOwnPtr<SourceLocation>, ScriptArguments*);

    MessageSource m_source;
    MessageLevel m_level;
    MessageType m_type;
    String m_message;
    OwnPtr<SourceLocation> m_location;
    Member<ScriptArguments> m_scriptArguments;
    unsigned long m_requestIdentifier;
    double m_timestamp;
    unsigned m_messageId;
    unsigned m_relatedMessageId;
};

} // namespace blink

#endif // ConsoleMessage_h
