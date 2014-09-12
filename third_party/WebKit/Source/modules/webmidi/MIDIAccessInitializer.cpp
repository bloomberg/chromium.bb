// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/webmidi/MIDIAccessInitializer.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMError.h"
#include "core/dom/Document.h"
#include "core/frame/Navigator.h"
#include "modules/webmidi/MIDIAccess.h"
#include "modules/webmidi/MIDIController.h"
#include "modules/webmidi/MIDIOptions.h"
#include "modules/webmidi/MIDIPort.h"

namespace blink {

MIDIAccessInitializer::MIDIAccessInitializer(ScriptState* scriptState, const MIDIOptions& options)
    : ScriptPromiseResolver(scriptState)
    , m_requestSysex(false)
{
    if (options.hasSysex())
        m_requestSysex = options.sysex();
}

MIDIAccessInitializer::~MIDIAccessInitializer()
{
    // It is safe to cancel a request which is already finished or canceld.
    Document* document = toDocument(executionContext());
    ASSERT(document);
    MIDIController* controller = MIDIController::from(document->frame());
    if (controller)
        controller->cancelSysexPermissionRequest(this);
}

ScriptPromise MIDIAccessInitializer::start()
{
    ScriptPromise promise = this->promise();
    m_accessor = MIDIAccessor::create(this);

    if (!m_requestSysex) {
        m_accessor->startSession();
        return promise;
    }
    Document* document = toDocument(executionContext());
    ASSERT(document);
    MIDIController* controller = MIDIController::from(document->frame());
    if (controller) {
        controller->requestSysexPermission(this);
    } else {
        reject(DOMError::create("SecurityError"));
    }
    return promise;
}

void MIDIAccessInitializer::didAddInputPort(const String& id, const String& manufacturer, const String& name, const String& version)
{
    ASSERT(m_accessor);
    m_portDescriptors.append(PortDescriptor(id, manufacturer, name, MIDIPort::MIDIPortTypeInput, version));
}

void MIDIAccessInitializer::didAddOutputPort(const String& id, const String& manufacturer, const String& name, const String& version)
{
    ASSERT(m_accessor);
    m_portDescriptors.append(PortDescriptor(id, manufacturer, name, MIDIPort::MIDIPortTypeOutput, version));
}

void MIDIAccessInitializer::didStartSession(bool success, const String& error, const String& message)
{
    ASSERT(m_accessor);
    if (success) {
        resolve(MIDIAccess::create(m_accessor.release(), m_requestSysex, m_portDescriptors, executionContext()));
    } else {
        reject(DOMError::create(error, message));
    }
}

void MIDIAccessInitializer::resolveSysexPermission(bool allowed)
{
    if (allowed)
        m_accessor->startSession();
    else
        reject(DOMError::create("SecurityError"));
}

SecurityOrigin* MIDIAccessInitializer::securityOrigin() const
{
    return executionContext()->securityOrigin();
}

ExecutionContext* MIDIAccessInitializer::executionContext() const
{
    return scriptState()->executionContext();
}

} // namespace blink
