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
#include "modules/webmidi/MIDIAccessPromise.h"

#include "core/dom/DOMError.h"
#include "modules/webmidi/MIDIAccess.h"
#include "modules/webmidi/MIDIErrorCallback.h"
#include "modules/webmidi/MIDISuccessCallback.h"

namespace WebCore {

PassRefPtr<MIDIAccessPromise> MIDIAccessPromise::create(ScriptExecutionContext* context, const Dictionary& options)
{
    RefPtr<MIDIAccessPromise> midiAccessPromise(adoptRef(new MIDIAccessPromise(context, options)));
    midiAccessPromise->suspendIfNeeded();
    return midiAccessPromise.release();
}

MIDIAccessPromise::MIDIAccessPromise(ScriptExecutionContext* context, const Dictionary& options)
    : ActiveDOMObject(context)
    , m_state(Pending)
    , m_options(adoptPtr(new MIDIOptions(options)))
{
    ScriptWrappable::init(this);
}

MIDIAccessPromise::~MIDIAccessPromise()
{
    stop();
}

void MIDIAccessPromise::fulfill()
{
    if (m_state == Pending) {
        if (m_successCallback) {
            m_state = Invoked;
            ASSERT(m_access.get());
            m_successCallback->handleEvent(m_access.release().leakRef(), m_options->sysex);
            m_options.clear();
        } else {
            m_state = Accepted;
        }
        unsetPendingActivity(this);
    }
    m_successCallback.clear();
    m_errorCallback.clear();
}

void MIDIAccessPromise::reject(PassRefPtr<DOMError> error)
{
    if (m_state == Pending) {
        if (m_errorCallback) {
            m_state = Invoked;
            m_errorCallback->handleEvent(error.leakRef());
        } else {
            m_state = Rejected;
            m_error = error;
        }
        unsetPendingActivity(this);
    }
    m_successCallback.clear();
    m_errorCallback.clear();
}

void MIDIAccessPromise::then(PassRefPtr<MIDISuccessCallback> successCallback, PassRefPtr<MIDIErrorCallback> errorCallback)
{
    // Lazily request access.
    if (!m_access) {
        setPendingActivity(this);
        m_access = MIDIAccess::create(scriptExecutionContext(), this);
    }

    switch (m_state) {
    case Accepted:
        successCallback->handleEvent(m_access.release().leakRef(), m_options->sysex);
        m_options.clear();
        m_state = Invoked;
        break;
    case Rejected:
        errorCallback->handleEvent(m_error.release().leakRef());
        m_state = Invoked;
        break;
    case Pending:
        m_successCallback = successCallback;
        m_errorCallback = errorCallback;
        break;
    case Invoked:
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

} // namespace WebCore
