// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/webmidi/MIDIAccessInitializer.h"

#include "bindings/v8/ScriptFunction.h"
#include "bindings/v8/ScriptPromise.h"
#include "bindings/v8/ScriptPromiseResolverWithContext.h"
#include "core/dom/DOMError.h"
#include "core/dom/Document.h"
#include "modules/webmidi/MIDIAccess.h"
#include "modules/webmidi/MIDIController.h"
#include "modules/webmidi/MIDIOptions.h"

namespace WebCore {

class MIDIAccessInitializer::PostAction : public ScriptFunction {
public:
    static PassOwnPtr<MIDIAccessInitializer::PostAction> create(v8::Isolate* isolate, WeakPtr<MIDIAccessInitializer> owner, State state) { return adoptPtr(new PostAction(isolate, owner, state)); }

private:
    PostAction(v8::Isolate* isolate, WeakPtr<MIDIAccessInitializer> owner, State state): ScriptFunction(isolate), m_owner(owner), m_state(state) { }
    virtual ScriptValue call(ScriptValue value) OVERRIDE
    {
        if (!m_owner.get())
            return value;
        m_owner->doPostAction(m_state);
        return value;
    }

    WeakPtr<MIDIAccessInitializer> m_owner;
    State m_state;
};

MIDIAccessInitializer::~MIDIAccessInitializer()
{
    ASSERT(m_state != Requesting);
}

MIDIAccessInitializer::MIDIAccessInitializer(const MIDIOptions& options, MIDIAccess* access)
    : m_state(Requesting)
    , m_weakPtrFactory(this)
    , m_options(options)
    , m_sysexEnabled(false)
    , m_access(access)
{
    m_accessor = MIDIAccessor::create(this);
}

void MIDIAccessInitializer::didAddInputPort(const String& id, const String& manufacturer, const String& name, const String& version)
{
    m_access->didAddInputPort(id, manufacturer, name, version);
}

void MIDIAccessInitializer::didAddOutputPort(const String& id, const String& manufacturer, const String& name, const String& version)
{
    m_access->didAddOutputPort(id, manufacturer, name, version);
}

void MIDIAccessInitializer::didStartSession(bool success, const String& error, const String& message)
{
    if (success)
        m_resolver->resolve(m_access);
    else
        m_resolver->reject(DOMError::create(error, message));
}

void MIDIAccessInitializer::setSysexEnabled(bool enable)
{
    m_sysexEnabled = enable;
    if (enable)
        m_accessor->startSession();
    else
        m_resolver->reject(DOMError::create("SecurityError"));
}

SecurityOrigin* MIDIAccessInitializer::securityOrigin() const
{
    return m_access->executionContext()->securityOrigin();
}

void MIDIAccessInitializer::cancel()
{
    if (m_state != Requesting)
        return;
    m_accessor.clear();
    m_weakPtrFactory.revokeAll();
    Document* document = toDocument(executionContext());
    ASSERT(document);
    MIDIController* controller = MIDIController::from(document->frame());
    ASSERT(controller);
    controller->cancelSysexPermissionRequest(this);
    m_state = Stopped;
}

ExecutionContext* MIDIAccessInitializer::executionContext() const
{
    return m_access->executionContext();
}

void MIDIAccessInitializer::permissionDenied()
{
    ASSERT(isMainThread());
    m_resolver->reject(DOMError::create("SecurityError"));
}

ScriptPromise MIDIAccessInitializer::initialize(ScriptState* scriptState)
{
    m_resolver = ScriptPromiseResolverWithContext::create(scriptState);
    ScriptPromise promise = m_resolver->promise();
    promise.then(PostAction::create(scriptState->isolate(), m_weakPtrFactory.createWeakPtr(), Resolved),
        PostAction::create(scriptState->isolate(), m_weakPtrFactory.createWeakPtr(), Stopped));

    if (!m_options.sysex) {
        m_accessor->startSession();
        return promise;
    }
    Document* document = toDocument(executionContext());
    ASSERT(document);
    MIDIController* controller = MIDIController::from(document->frame());
    if (controller) {
        controller->requestSysexPermission(this);
    } else {
        m_resolver->reject(DOMError::create("SecurityError"));
    }
    return promise;
}

void MIDIAccessInitializer::doPostAction(State state)
{
    ASSERT(m_state == Requesting);
    ASSERT(state == Resolved || state == Stopped);
    if (state == Resolved) {
        m_access->initialize(m_accessor.release(), m_sysexEnabled);
    }
    m_accessor.clear();
    m_weakPtrFactory.revokeAll();
    m_state = state;
}

} // namespace WebCore
