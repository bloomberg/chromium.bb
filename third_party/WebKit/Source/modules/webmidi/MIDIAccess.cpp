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
#include "modules/webmidi/MIDIAccess.h"

#include "core/dom/DOMError.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/loader/DocumentLoadTiming.h"
#include "core/loader/DocumentLoader.h"
#include "modules/webmidi/MIDIAccessPromise.h"
#include "modules/webmidi/MIDIConnectionEvent.h"
#include "modules/webmidi/MIDIInput.h"
#include "modules/webmidi/MIDIOutput.h"
#include "modules/webmidi/MIDIPort.h"

namespace WebCore {

PassRefPtr<MIDIAccess> MIDIAccess::create(ScriptExecutionContext* context, MIDIAccessPromise* promise)
{
    RefPtr<MIDIAccess> midiAccess(adoptRef(new MIDIAccess(context, promise)));
    midiAccess->suspendIfNeeded();
    return midiAccess.release();
}

MIDIAccess::~MIDIAccess()
{
    stop();
}

MIDIAccess::MIDIAccess(ScriptExecutionContext* context, MIDIAccessPromise* promise)
    : ActiveDOMObject(context)
    , m_promise(promise)
    , m_hasAccess(false)
{
    ScriptWrappable::init(this);
    m_accessor = MIDIAccessor::create(this);
    m_accessor->requestAccess(promise->options()->sysex);
}

void MIDIAccess::didAddInputPort(const String& id, const String& manufacturer, const String& name, const String& version)
{
    ASSERT(isMainThread());

    m_inputs.append(MIDIInput::create(scriptExecutionContext(), id, manufacturer, name, version));
}

void MIDIAccess::didAddOutputPort(const String& id, const String& manufacturer, const String& name, const String& version)
{
    ASSERT(isMainThread());

    m_outputs.append(MIDIOutput::create(scriptExecutionContext(), id, manufacturer, name, version));
}

void MIDIAccess::didAllowAccess()
{
    ASSERT(isMainThread());

    m_hasAccess = true;
    m_promise->fulfill();
}

void MIDIAccess::didBlockAccess()
{
    ASSERT(isMainThread());

    m_hasAccess = false;
    RefPtr<DOMError> error = DOMError::create("SecurityError");
    m_promise->reject(error);
}

void MIDIAccess::didReceiveMIDIData(unsigned portIndex, const unsigned char* data, size_t length, double timeStamp)
{
    ASSERT(isMainThread());

    if (m_hasAccess && portIndex < m_inputs.size()) {
        // Convert from time in seconds which is based on the time coordinate system of monotonicallyIncreasingTime()
        // into time in milliseconds (a DOMHighResTimeStamp) according to the same time coordinate system as performance.now().
        // This is how timestamps are defined in the Web MIDI spec.
        Document* document = toDocument(scriptExecutionContext());
        ASSERT(document);

        double timeStampInMilliseconds = 1000 * document->loader()->timing()->monotonicTimeToZeroBasedDocumentTime(timeStamp);

        m_inputs[portIndex]->didReceiveMIDIData(portIndex, data, length, timeStampInMilliseconds);
    }
}

void MIDIAccess::stop()
{
    m_hasAccess = false;
}

} // namespace WebCore
