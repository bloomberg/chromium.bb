/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010-2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef InspectorDebuggerAgent_h
#define InspectorDebuggerAgent_h

#include "core/CoreExport.h"
#include "core/inspector/V8DebuggerAgent.h"

namespace blink {

class CORE_EXPORT InspectorDebuggerAgent
    : public V8DebuggerAgent
    , public V8DebuggerAgent::Client {
public:
    ~InspectorDebuggerAgent() override;
    DECLARE_VIRTUAL_TRACE();

    void enable(ErrorString*) override;

    // V8DebuggerAgent::Client implementation.
    void startListeningV8Debugger() override;
    void stopListeningV8Debugger() override;
    bool canPauseOnPromiseEvent() final;
    void didCreatePromise() final;
    void didResolvePromise() final;
    void didRejectPromise() final;

    class CORE_EXPORT Listener : public WillBeGarbageCollectedMixin {
    public:
        virtual ~Listener() { }
        virtual bool canPauseOnPromiseEvent() = 0;
        virtual void didCreatePromise() = 0;
        virtual void didResolvePromise() = 0;
        virtual void didRejectPromise() = 0;
    };
    void setListener(Listener* listener) { m_listener = listener; }

protected:
    InspectorDebuggerAgent(InjectedScriptManager*, V8Debugger*);

private:
    RawPtrWillBeMember<Listener> m_listener;
};

} // namespace blink


#endif // !defined(InspectorDebuggerAgent_h)
