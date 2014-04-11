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

#include "bindings/v8/ScriptFunction.h"
#include "bindings/v8/ScriptPromise.h"
#include "bindings/v8/ScriptPromiseResolverWithContext.h"
#include "bindings/v8/V8Binding.h"
#include "core/dom/DOMError.h"
#include "core/dom/Document.h"
#include "core/loader/DocumentLoadTiming.h"
#include "core/loader/DocumentLoader.h"
#include "modules/webmidi/MIDIConnectionEvent.h"
#include "modules/webmidi/MIDIController.h"
#include "modules/webmidi/MIDIOptions.h"
#include "modules/webmidi/MIDIPort.h"
#include "platform/AsyncMethodRunner.h"

namespace WebCore {

class MIDIAccess::PostAction : public ScriptFunction {
public:
    static PassOwnPtr<MIDIAccess::PostAction> create(v8::Isolate* isolate, WeakPtr<MIDIAccess> owner, State state) { return adoptPtr(new PostAction(isolate, owner, state)); }

private:
    PostAction(v8::Isolate* isolate, WeakPtr<MIDIAccess> owner, State state): ScriptFunction(isolate), m_owner(owner), m_state(state) { }
    virtual ScriptValue call(ScriptValue value) OVERRIDE
    {
        if (!m_owner.get())
            return value;
        m_owner->doPostAction(m_state);
        return value;
    }

    WeakPtr<MIDIAccess> m_owner;
    State m_state;
};

ScriptPromise MIDIAccess::request(const MIDIOptions& options, ExecutionContext* context)
{
    RefPtrWillBeRawPtr<MIDIAccess> midiAccess(adoptRefWillBeRefCountedGarbageCollected(new MIDIAccess(options, context)));
    midiAccess->suspendIfNeeded();
    // Create a wrapper to expose this object to the V8 GC so that
    // hasPendingActivity takes effect.
    toV8NoInline(midiAccess.get(), context);
    // Now this object is retained because m_state equals to Requesting.
    return midiAccess->startRequest();
}

MIDIAccess::~MIDIAccess()
{
}

MIDIAccess::MIDIAccess(const MIDIOptions& options, ExecutionContext* context)
    : ActiveDOMObject(context)
    , m_state(Requesting)
    , m_weakPtrFactory(this)
    , m_options(options)
    , m_sysexEnabled(false)
{
    ScriptWrappable::init(this);
    m_accessor = MIDIAccessor::create(this);
}

void MIDIAccess::setSysexEnabled(bool enable)
{
    m_sysexEnabled = enable;
    if (enable) {
        m_accessor->startSession();
    } else {
        m_resolver->reject(DOMError::create("SecurityError"));
    }
}

void MIDIAccess::didAddInputPort(const String& id, const String& manufacturer, const String& name, const String& version)
{
    ASSERT(isMainThread());

    m_inputs.append(MIDIInput::create(this, id, manufacturer, name, version));
}

void MIDIAccess::didAddOutputPort(const String& id, const String& manufacturer, const String& name, const String& version)
{
    ASSERT(isMainThread());

    unsigned portIndex = m_outputs.size();
    m_outputs.append(MIDIOutput::create(this, portIndex, id, manufacturer, name, version));
}

void MIDIAccess::didStartSession(bool success)
{
    ASSERT(isMainThread());
    if (success)
        m_resolver->resolve(this);
    else
        m_resolver->reject(DOMError::create("InvalidStateError"));
}

void MIDIAccess::didReceiveMIDIData(unsigned portIndex, const unsigned char* data, size_t length, double timeStamp)
{
    ASSERT(isMainThread());

    if (m_state == Resolved && portIndex < m_inputs.size()) {
        // Convert from time in seconds which is based on the time coordinate system of monotonicallyIncreasingTime()
        // into time in milliseconds (a DOMHighResTimeStamp) according to the same time coordinate system as performance.now().
        // This is how timestamps are defined in the Web MIDI spec.
        Document* document = toDocument(executionContext());
        ASSERT(document);

        double timeStampInMilliseconds = 1000 * document->loader()->timing()->monotonicTimeToZeroBasedDocumentTime(timeStamp);

        m_inputs[portIndex]->didReceiveMIDIData(portIndex, data, length, timeStampInMilliseconds);
    }
}

void MIDIAccess::sendMIDIData(unsigned portIndex, const unsigned char* data, size_t length, double timeStampInMilliseconds)
{
    if (m_state == Resolved && portIndex < m_outputs.size() && data && length > 0) {
        // Convert from a time in milliseconds (a DOMHighResTimeStamp) according to the same time coordinate system as performance.now()
        // into a time in seconds which is based on the time coordinate system of monotonicallyIncreasingTime().
        double timeStamp;

        if (!timeStampInMilliseconds) {
            // We treat a value of 0 (which is the default value) as special, meaning "now".
            // We need to translate it exactly to 0 seconds.
            timeStamp = 0;
        } else {
            Document* document = toDocument(executionContext());
            ASSERT(document);
            double documentStartTime = document->loader()->timing()->referenceMonotonicTime();
            timeStamp = documentStartTime + 0.001 * timeStampInMilliseconds;
        }

        m_accessor->sendMIDIData(portIndex, data, length, timeStamp);
    }
}

void MIDIAccess::stop()
{
    if (m_state == Stopped)
        return;
    m_accessor.clear();
    m_weakPtrFactory.revokeAll();
    if (m_state == Requesting) {
        Document* document = toDocument(executionContext());
        ASSERT(document);
        MIDIController* controller = MIDIController::from(document->page());
        ASSERT(controller);
        controller->cancelSysexPermissionRequest(this);
    }
    m_state = Stopped;
}

bool MIDIAccess::hasPendingActivity() const
{
    return m_state == Requesting;
}

void MIDIAccess::permissionDenied()
{
    ASSERT(isMainThread());
    m_resolver->reject(DOMError::create("SecurityError"));
}

ScriptPromise MIDIAccess::startRequest()
{
    m_resolver = ScriptPromiseResolverWithContext::create(NewScriptState::current(toIsolate(executionContext())));
    ScriptPromise promise = m_resolver->promise();
    promise.then(PostAction::create(toIsolate(executionContext()), m_weakPtrFactory.createWeakPtr(), Resolved),
        PostAction::create(toIsolate(executionContext()), m_weakPtrFactory.createWeakPtr(), Stopped));

    if (!m_options.sysex) {
        m_accessor->startSession();
        return promise;
    }
    Document* document = toDocument(executionContext());
    ASSERT(document);
    MIDIController* controller = MIDIController::from(document->page());
    if (controller) {
        controller->requestSysexPermission(this);
    } else {
        m_resolver->reject(DOMError::create("SecurityError"));
    }
    return promise;
}

void MIDIAccess::doPostAction(State state)
{
    ASSERT(m_state == Requesting);
    ASSERT(state == Resolved || state == Stopped);
    if (state == Stopped) {
        m_accessor.clear();
    }
    m_weakPtrFactory.revokeAll();
    m_state = state;
}

void MIDIAccess::trace(Visitor* visitor)
{
    visitor->trace(m_inputs);
    visitor->trace(m_outputs);
}

} // namespace WebCore
