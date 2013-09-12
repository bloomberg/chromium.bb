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
#include "modules/webmidi/MIDIInput.h"

#include "modules/webmidi/MIDIAccess.h"
#include "modules/webmidi/MIDIMessageEvent.h"

namespace WebCore {

PassRefPtr<MIDIInput> MIDIInput::create(MIDIAccess* access, ScriptExecutionContext* context, const String& id, const String& manufacturer, const String& name, const String& version)
{
    ASSERT(access);
    return adoptRef(new MIDIInput(access, context, id, manufacturer, name, version));
}

MIDIInput::MIDIInput(MIDIAccess* access, ScriptExecutionContext* context, const String& id, const String& manufacturer, const String& name, const String& version)
    : MIDIPort(context, id, manufacturer, name, MIDIPortTypeInput, version)
    , m_access(access)
{
    ScriptWrappable::init(this);
}

void MIDIInput::didReceiveMIDIData(unsigned portIndex, const unsigned char* data, size_t length, double timeStamp)
{
    ASSERT(isMainThread());

    // The received MIDI data may contain one or more messages.
    // The Web MIDI API requires that a separate event be dispatched for each message,
    // so we walk through the data and dispatch one at a time.
    size_t i = 0;
    while (i < length) {
        unsigned char status = data[i];
        unsigned char strippedStatus = status & 0xf0;

        // FIXME: integrate sending side filtering and implement more extensive filtering.
        if (strippedStatus >= 0xf0 && !m_access->sysExEnabled())
            break;

        // All non System Exclusive messages have a total size of 3 except for Program Change and Channel Pressure.
        size_t totalMessageSize = (strippedStatus == 0xc0 || strippedStatus == 0xd0) ? 2 : 3;

        if (i + totalMessageSize <= length) {
            RefPtr<Uint8Array> array = Uint8Array::create(totalMessageSize);
            array->setRange(data + i, totalMessageSize, 0);

            dispatchEvent(MIDIMessageEvent::create(timeStamp, array));
        }

        i += totalMessageSize;
    }
}

} // namespace WebCore
