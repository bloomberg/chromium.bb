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
#include "modules/webmidi/MIDIOutput.h"

#include "bindings/v8/ExceptionMessages.h"
#include "bindings/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "modules/webmidi/MIDIAccess.h"

namespace WebCore {

PassRefPtr<MIDIOutput> MIDIOutput::create(MIDIAccess* access, unsigned portIndex, ScriptExecutionContext* context, const String& id, const String& manufacturer, const String& name, const String& version)
{
    ASSERT(access);
    return adoptRef(new MIDIOutput(access, portIndex, context, id, manufacturer, name, version));
}

MIDIOutput::MIDIOutput(MIDIAccess* access, unsigned portIndex, ScriptExecutionContext* context, const String& id, const String& manufacturer, const String& name, const String& version)
    : MIDIPort(context, id, manufacturer, name, MIDIPortTypeOutput, version)
    , m_access(access)
    , m_portIndex(portIndex)
{
    ScriptWrappable::init(this);
}

MIDIOutput::~MIDIOutput()
{
}

void MIDIOutput::send(Uint8Array* array, double timestamp, ExceptionState& es)
{
    if (!array)
        return;

    const unsigned char* data = array->data();
    size_t length = array->length();

    // Filter out System Exclusive messages if we're not allowed.
    // FIXME: implement more extensive filtering.
    if (length > 0 && data[0] >= 0xf0 && !m_access->sysExEnabled()) {
        es.throwSecurityError(ExceptionMessages::failedToExecute("send", "MIDIOutput", "permission to send system exclusive messages is denied."));
        return;
    }

    m_access->sendMIDIData(m_portIndex, data, length, timestamp);
}

void MIDIOutput::send(Vector<unsigned> unsignedData, double timestamp, ExceptionState& es)
{
    RefPtr<Uint8Array> array = Uint8Array::create(unsignedData.size());

    for (size_t i = 0; i < unsignedData.size(); ++i) {
        if (unsignedData[i] > 0xff) {
            es.throwDOMException(InvalidStateError);
            return;
        }
        unsigned char value = unsignedData[i] & 0xff;
        array->set(i, value);
    }

    send(array.get(), timestamp, es);
}

void MIDIOutput::send(Uint8Array* data, ExceptionState& es)
{
    send(data, 0, es);
}

void MIDIOutput::send(Vector<unsigned> unsignedData, ExceptionState& es)
{
    send(unsignedData, 0, es);
}

} // namespace WebCore
