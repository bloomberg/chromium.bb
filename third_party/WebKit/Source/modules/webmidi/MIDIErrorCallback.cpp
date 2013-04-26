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
#include "modules/webmidi/MIDIErrorCallback.h"

#include "core/dom/DOMError.h"
#include "core/dom/ScriptExecutionContext.h"

namespace WebCore {

namespace {

class DispatchCallbackTask : public ScriptExecutionContext::Task {
public:
    static PassOwnPtr<DispatchCallbackTask> create(PassRefPtr<MIDIErrorCallback> callback, PassRefPtr<DOMError> error)
    {
        return adoptPtr(new DispatchCallbackTask(callback, error));
    }

    virtual void performTask(ScriptExecutionContext*)
    {
        m_callback->handleEvent(m_error.get());
    }

private:
    DispatchCallbackTask(PassRefPtr<MIDIErrorCallback> callback, PassRefPtr<DOMError> error)
            : m_callback(callback)
            , m_error(error)
    {
    }

    RefPtr<MIDIErrorCallback> m_callback;
    RefPtr<DOMError> m_error;
};

} // namespace

void MIDIErrorCallback::scheduleCallback(ScriptExecutionContext* context, PassRefPtr<DOMError> error)
{
    context->postTask(DispatchCallbackTask::create(this, error));
}

} // namespace WebCore
