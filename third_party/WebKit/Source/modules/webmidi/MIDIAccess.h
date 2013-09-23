/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MIDIAccess_h
#define MIDIAccess_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/events/EventTarget.h"
#include "core/platform/midi/MIDIAccessor.h"
#include "core/platform/midi/MIDIAccessorClient.h"
#include "modules/webmidi/MIDIInput.h"
#include "modules/webmidi/MIDIOutput.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"

namespace WebCore {

class ScriptExecutionContext;
class MIDIAccessPromise;

class MIDIAccess : public RefCounted<MIDIAccess>, public ScriptWrappable, public ActiveDOMObject, public EventTarget, public MIDIAccessorClient {
public:
    virtual ~MIDIAccess();
    static PassRefPtr<MIDIAccess> create(ScriptExecutionContext*, MIDIAccessPromise*);

    MIDIInputVector inputs() const { return m_inputs; }
    MIDIOutputVector outputs() const { return m_outputs; }

    using RefCounted<MIDIAccess>::ref;
    using RefCounted<MIDIAccess>::deref;

    DEFINE_ATTRIBUTE_EVENT_LISTENER(connect);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(disconnect);

    void setSysExEnabled(bool);
    bool sysExEnabled() const { return m_sysExEnabled; }

    // EventTarget
    virtual const AtomicString& interfaceName() const OVERRIDE { return eventNames().interfaceForMIDIAccess; }
    virtual ScriptExecutionContext* scriptExecutionContext() const OVERRIDE { return ActiveDOMObject::scriptExecutionContext(); }

    // ActiveDOMObject
    virtual bool canSuspend() const OVERRIDE { return true; }
    virtual void stop();

    // MIDIAccessorClient
    virtual void didAddInputPort(const String& id, const String& manufacturer, const String& name, const String& version) OVERRIDE;
    virtual void didAddOutputPort(const String& id, const String& manufacturer, const String& name, const String& version) OVERRIDE;
    virtual void didStartSession(bool success) OVERRIDE;
    virtual void didReceiveMIDIData(unsigned portIndex, const unsigned char* data, size_t length, double timeStamp) OVERRIDE;

    // |timeStampInMilliseconds| is in the same time coordinate system as performance.now().
    void sendMIDIData(unsigned portIndex, const unsigned char* data, size_t length, double timeStampInMilliseconds);

private:
    MIDIAccess(ScriptExecutionContext*, MIDIAccessPromise*);

    void startRequest();
    virtual void permissionDenied();

    // EventTarget
    virtual void refEventTarget() OVERRIDE { ref(); }
    virtual void derefEventTarget() OVERRIDE { deref(); }
    virtual EventTargetData* eventTargetData() OVERRIDE { return &m_eventTargetData; }
    virtual EventTargetData* ensureEventTargetData() OVERRIDE { return &m_eventTargetData; }

    MIDIInputVector m_inputs;
    MIDIOutputVector m_outputs;
    EventTargetData m_eventTargetData;
    MIDIAccessPromise* m_promise;

    OwnPtr<MIDIAccessor> m_accessor;
    bool m_hasAccess;
    bool m_sysExEnabled;
    bool m_requesting;
};

} // namespace WebCore

#endif // MIDIAccess_h
