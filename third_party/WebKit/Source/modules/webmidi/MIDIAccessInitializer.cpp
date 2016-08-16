// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webmidi/MIDIAccessInitializer.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Navigator.h"
#include "modules/webmidi/MIDIAccess.h"
#include "modules/webmidi/MIDIOptions.h"
#include "modules/webmidi/MIDIPort.h"
#include "platform/UserGestureIndicator.h"
#include "platform/mojo/MojoHelper.h"
#include "public/platform/InterfaceProvider.h"
#include "third_party/WebKit/public/platform/modules/permissions/permission.mojom-blink.h"

namespace blink {

using PortState = WebMIDIAccessorClient::MIDIPortState;

using mojom::blink::PermissionName;
using mojom::blink::PermissionStatus;

MIDIAccessInitializer::MIDIAccessInitializer(ScriptState* scriptState, const MIDIOptions& options)
    : ScriptPromiseResolver(scriptState)
    , m_options(options)
{
}

void MIDIAccessInitializer::contextDestroyed()
{
    m_permissionService.reset();
    LifecycleObserver::contextDestroyed();
}

ScriptPromise MIDIAccessInitializer::start()
{
    ScriptPromise promise = this->promise();
    m_accessor = MIDIAccessor::create(this);

    Document* document = toDocument(getExecutionContext());
    DCHECK(document);

    document->frame()->interfaceProvider()->getInterface(mojo::GetProxy(&m_permissionService));

    bool requestSysEx = m_options.hasSysex() && m_options.sysex();
    Vector<PermissionName> permissions;
    permissions.resize(requestSysEx ? 2 : 1);

    permissions[0] = PermissionName::MIDI;
    if (requestSysEx)
        permissions[1] = PermissionName::MIDI_SYSEX;

    m_permissionService->RequestPermissions(
        permissions,
        getExecutionContext()->getSecurityOrigin(),
        UserGestureIndicator::processingUserGesture(),
        convertToBaseCallback(WTF::bind(&MIDIAccessInitializer::onPermissionsUpdated, wrapPersistent(this))));

    return promise;
}

void MIDIAccessInitializer::didAddInputPort(const String& id, const String& manufacturer, const String& name, const String& version, PortState state)
{
    DCHECK(m_accessor);
    m_portDescriptors.append(PortDescriptor(id, manufacturer, name, MIDIPort::TypeInput, version, state));
}

void MIDIAccessInitializer::didAddOutputPort(const String& id, const String& manufacturer, const String& name, const String& version, PortState state)
{
    DCHECK(m_accessor);
    m_portDescriptors.append(PortDescriptor(id, manufacturer, name, MIDIPort::TypeOutput, version, state));
}

void MIDIAccessInitializer::didSetInputPortState(unsigned portIndex, PortState state)
{
    // didSetInputPortState() is not allowed to call before didStartSession()
    // is called. Once didStartSession() is called, MIDIAccessorClient methods
    // are delegated to MIDIAccess. See constructor of MIDIAccess.
    NOTREACHED();
}

void MIDIAccessInitializer::didSetOutputPortState(unsigned portIndex, PortState state)
{
    // See comments on didSetInputPortState().
    NOTREACHED();
}

void MIDIAccessInitializer::didStartSession(bool success, const String& error, const String& message)
{
    DCHECK(m_accessor);
    if (success) {
        resolve(MIDIAccess::create(std::move(m_accessor), m_options.hasSysex() && m_options.sysex(), m_portDescriptors, getExecutionContext()));
    } else {
        // The spec says the name is one of
        //  - SecurityError
        //  - AbortError
        //  - InvalidStateError
        //  - NotSupportedError
        // TODO(toyoshim): Do not rely on |error| string. Instead an enum
        // representing an ExceptionCode should be defined and deliverred.
        ExceptionCode ec = InvalidStateError;
        if (error == DOMException::getErrorName(SecurityError)) {
            ec = SecurityError;
        } else if (error == DOMException::getErrorName(AbortError)) {
            ec = AbortError;
        } else if (error == DOMException::getErrorName(InvalidStateError)) {
            ec = InvalidStateError;
        } else if (error == DOMException::getErrorName(NotSupportedError)) {
            ec = NotSupportedError;
        }
        reject(DOMException::create(ec, message));
    }
}

ExecutionContext* MIDIAccessInitializer::getExecutionContext() const
{
    return getScriptState()->getExecutionContext();
}

void MIDIAccessInitializer::onPermissionsUpdated(const Vector<PermissionStatus>& statusArray)
{
    bool allowed = true;
    for (const auto status : statusArray) {
        if (status != PermissionStatus::GRANTED) {
            allowed = false;
            break;
        }
    }
    m_permissionService.reset();
    if (allowed)
        m_accessor->startSession();
    else
        reject(DOMException::create(SecurityError));

}

void MIDIAccessInitializer::onPermissionUpdated(PermissionStatus status)
{
    m_permissionService.reset();
    if (status == PermissionStatus::GRANTED)
        m_accessor->startSession();
    else
        reject(DOMException::create(SecurityError));
}

} // namespace blink
