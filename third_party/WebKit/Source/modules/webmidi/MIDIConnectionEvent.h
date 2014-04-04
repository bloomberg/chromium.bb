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

#ifndef MIDIConnectionEvent_h
#define MIDIConnectionEvent_h

#include "core/events/Event.h"
#include "modules/webmidi/MIDIPort.h"

namespace WebCore {

struct MIDIConnectionEventInit : public EventInit {
    MIDIConnectionEventInit()
        : port(nullptr)
    {
    };

    RefPtr<MIDIPort> port;
};

class MIDIConnectionEvent FINAL : public Event {
public:
    static PassRefPtrWillBeRawPtr<MIDIConnectionEvent> create()
    {
        return adoptRefWillBeNoop(new MIDIConnectionEvent());
    }

    static PassRefPtrWillBeRawPtr<MIDIConnectionEvent> create(const AtomicString& type, PassRefPtr<MIDIPort> port)
    {
        return adoptRefWillBeNoop(new MIDIConnectionEvent(type, port));
    }

    static PassRefPtrWillBeRawPtr<MIDIConnectionEvent> create(const AtomicString& type, const MIDIConnectionEventInit& initializer)
    {
        return adoptRefWillBeNoop(new MIDIConnectionEvent(type, initializer));
    }

    RefPtr<MIDIPort> port() { return m_port; }

    virtual const AtomicString& interfaceName() const OVERRIDE { return EventNames::MIDIConnectionEvent; }

    virtual void trace(Visitor* visitor) OVERRIDE { Event::trace(visitor); }

private:
    MIDIConnectionEvent()
    {
        ScriptWrappable::init(this);
    }

    MIDIConnectionEvent(const AtomicString& type, PassRefPtr<MIDIPort> port)
        : Event(type, false, false)
        , m_port(port)
    {
        ScriptWrappable::init(this);
    }

    MIDIConnectionEvent(const AtomicString& type, const MIDIConnectionEventInit& initializer)
        : Event(type, initializer)
        , m_port(initializer.port)
    {
        ScriptWrappable::init(this);
    }

    RefPtr<MIDIPort> m_port;
};

} // namespace WebCore

#endif // MIDIConnectionEvent_h
