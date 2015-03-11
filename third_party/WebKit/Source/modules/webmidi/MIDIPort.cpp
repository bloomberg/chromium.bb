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

#include "config.h"
#include "modules/webmidi/MIDIPort.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "core/dom/DOMException.h"
#include "modules/webmidi/MIDIAccess.h"
#include "modules/webmidi/MIDIConnectionEvent.h"

namespace blink {

using PortState = MIDIAccessor::MIDIPortState;

MIDIPort::MIDIPort(MIDIAccess* access, const String& id, const String& manufacturer, const String& name, MIDIPortTypeCode type, const String& version, PortState state)
    : m_id(id)
    , m_manufacturer(manufacturer)
    , m_name(name)
    , m_type(type)
    , m_version(version)
    , m_access(access)
{
    ASSERT(access);
    ASSERT(type == MIDIPortTypeInput || type == MIDIPortTypeOutput);
    ASSERT(state == PortState::MIDIPortStateDisconnected
        || state == PortState::MIDIPortStateConnected
        || state == PortState::MIDIPortStateOpened);
    // FIXME: Remove following code once blink API has a real open and close
    // operations.
    if (state == PortState::MIDIPortStateOpened)
        state = PortState::MIDIPortStateConnected;
    m_state = state;
}

String MIDIPort::state() const
{
    switch (m_state) {
    case PortState::MIDIPortStateDisconnected:
        return "disconnected";
    case PortState::MIDIPortStateConnected:
        return "connected";
    case PortState::MIDIPortStateOpened:
        return "opened";
    default:
        ASSERT_NOT_REACHED();
    }
    return emptyString();
}

String MIDIPort::type() const
{
    switch (m_type) {
    case MIDIPortTypeInput:
        return "input";
    case MIDIPortTypeOutput:
        return "output";
    default:
        ASSERT_NOT_REACHED();
    }
    return emptyString();
}

ScriptPromise MIDIPort::open(ScriptState* scriptState)
{
    switch (m_state) {
    case PortState::MIDIPortStateDisconnected:
        return reject(scriptState, InvalidStateError, "The port has been disconnected.");
    case PortState::MIDIPortStateConnected:
        // FIXME: Add blink API to perform a real open operation.
        setState(PortState::MIDIPortStateOpened);
        // fall through
    case PortState::MIDIPortStateOpened:
        return accept(scriptState);
    default:
        ASSERT_NOT_REACHED();
    }
    return reject(scriptState, InvalidStateError, "The port is in unknown state.");
}

ScriptPromise MIDIPort::close(ScriptState* scriptState)
{
    switch (m_state) {
    case PortState::MIDIPortStateDisconnected:
        return reject(scriptState, InvalidStateError, "The port has been disconnected.");
    case PortState::MIDIPortStateOpened:
        // FIXME: Add blink API to perform a real close operation.
        setState(PortState::MIDIPortStateConnected);
        // fall through
    case PortState::MIDIPortStateConnected:
        return accept(scriptState);
    default:
        ASSERT_NOT_REACHED();
    }
    return reject(scriptState, InvalidStateError, "The port is in unknown state.");
}

void MIDIPort::setState(PortState state)
{
    if (m_state == state)
        return;
    m_state = state;
    dispatchEvent(MIDIConnectionEvent::create(this));
}

ExecutionContext* MIDIPort::executionContext() const
{
    return m_access->executionContext();
}

DEFINE_TRACE(MIDIPort)
{
    visitor->trace(m_access);
    RefCountedGarbageCollectedEventTargetWithInlineData<MIDIPort>::trace(visitor);
}

ScriptPromise MIDIPort::accept(ScriptState* scriptState)
{
    return ScriptPromise::cast(scriptState, toV8(this, scriptState->context()->Global(), scriptState->isolate()));
}

ScriptPromise MIDIPort::reject(ScriptState* scriptState, ExceptionCode ec, const String& message)
{
    return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(ec, message));
}

} // namespace blink
