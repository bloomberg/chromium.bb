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

#ifndef MIDIPort_h
#define MIDIPort_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/events/EventTarget.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"

namespace WebCore {

class MIDIPort : public RefCounted<MIDIPort>, public ScriptWrappable, public ActiveDOMObject, public EventTarget {
public:
    enum MIDIPortTypeCode {
        MIDIPortTypeInput,
        MIDIPortTypeOutput
    };

    static PassRefPtr<MIDIPort> create(ScriptExecutionContext*, const String& id, const String& manufacturer, const String& name, MIDIPortTypeCode, const String& version);
    virtual ~MIDIPort();

    String id() const { return m_id; }
    String manufacturer() const { return m_manufacturer; }
    String name() const { return m_name; }
    String type() const;
    String version() const { return m_version; }

    using RefCounted<MIDIPort>::ref;
    using RefCounted<MIDIPort>::deref;

    DEFINE_ATTRIBUTE_EVENT_LISTENER(disconnect);

    // EventTarget
    virtual const AtomicString& interfaceName() const OVERRIDE { return eventNames().interfaceForMIDIPort; }
    virtual ScriptExecutionContext* scriptExecutionContext() const OVERRIDE { return ActiveDOMObject::scriptExecutionContext(); }

    // ActiveDOMObject
    virtual bool canSuspend() const OVERRIDE { return true; }

protected:
    MIDIPort(ScriptExecutionContext*, const String& id, const String& manufacturer, const String& name, MIDIPortTypeCode, const String& version);

private:
    // EventTarget
    virtual void refEventTarget() OVERRIDE { ref(); }
    virtual void derefEventTarget() OVERRIDE { deref(); }
    virtual EventTargetData* eventTargetData() OVERRIDE { return &m_eventTargetData; }
    virtual EventTargetData* ensureEventTargetData() OVERRIDE { return &m_eventTargetData; }

    String m_id;
    String m_manufacturer;
    String m_name;
    MIDIPortTypeCode m_type;
    String m_version;
    EventTargetData m_eventTargetData;
};

typedef Vector<RefPtr<MIDIPort> > MIDIPortVector;

} // namespace WebCore

#endif // MIDIPort_h
