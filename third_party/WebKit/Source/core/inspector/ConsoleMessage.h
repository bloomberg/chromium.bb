// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ConsoleMessage_h
#define ConsoleMessage_h

#include "bindings/core/v8/ScriptState.h"
#include "core/frame/ConsoleTypes.h"
#include "core/inspector/ScriptCallStack.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ScriptCallStack;
class ScriptState;

class ConsoleMessage FINAL: public RefCountedWillBeGarbageCollectedFinalized<ConsoleMessage> {
public:
    static PassRefPtrWillBeRawPtr<ConsoleMessage> create(MessageSource source, MessageLevel level, const String& message, const String& url = String(), unsigned lineNumber = 0, unsigned columnNumber = 0)
    {
        return adoptRefWillBeNoop(new ConsoleMessage(source, level, message, url, lineNumber, columnNumber));
    }

    ~ConsoleMessage();

    PassRefPtrWillBeRawPtr<ScriptCallStack> callStack() const;
    void setCallStack(PassRefPtrWillBeRawPtr<ScriptCallStack>);
    ScriptState* scriptState() const;
    void setScriptState(ScriptState*);
    unsigned long requestIdentifier() const;
    void setRequestIdentifier(unsigned long);
    const String& url() const;
    void setURL(const String&);
    unsigned lineNumber() const;
    void setLineNumber(unsigned);

    MessageSource source() const;
    MessageLevel level() const;
    const String& message() const;
    unsigned columnNumber() const;

    void trace(Visitor*);

private:
    ConsoleMessage();
    ConsoleMessage(MessageSource, MessageLevel, const String& message, const String& url = String(), unsigned lineNumber = 0, unsigned columnNumber = 0);

    MessageSource m_source;
    MessageLevel m_level;
    String m_message;
    String m_url;
    unsigned m_lineNumber;
    unsigned m_columnNumber;
    RefPtrWillBeMember<ScriptCallStack> m_callStack;
    ScriptState* m_scriptState;
    unsigned long m_requestIdentifier;
};

} // namespace WebCore

#endif // ConsoleMessage_h
