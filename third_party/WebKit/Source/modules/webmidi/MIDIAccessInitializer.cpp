// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webmidi/MIDIAccessInitializer.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/UserGestureIndicator.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Navigator.h"
#include "modules/permissions/PermissionUtils.h"
#include "modules/webmidi/MIDIAccess.h"
#include "modules/webmidi/MIDIOptions.h"
#include "modules/webmidi/MIDIPort.h"
#include "platform/mojo/MojoHelper.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/modules/permissions/permission.mojom-blink.h"

namespace blink {

using midi::mojom::PortState;
using midi::mojom::Result;
using mojom::blink::PermissionStatus;

MIDIAccessInitializer::MIDIAccessInitializer(ScriptState* script_state,
                                             const MIDIOptions& options)
    : ScriptPromiseResolver(script_state), options_(options) {}

void MIDIAccessInitializer::ContextDestroyed(ExecutionContext*) {
  permission_service_.reset();
}

ScriptPromise MIDIAccessInitializer::Start() {
  ScriptPromise promise = this->Promise();
  accessor_ = MIDIAccessor::Create(this);

  ConnectToPermissionService(GetExecutionContext(),
                             mojo::MakeRequest(&permission_service_));
  permission_service_->RequestPermission(
      CreateMidiPermissionDescriptor(options_.hasSysex() && options_.sysex()),
      GetExecutionContext()->GetSecurityOrigin(),
      UserGestureIndicator::ProcessingUserGesture(),
      ConvertToBaseCallback(WTF::Bind(
          &MIDIAccessInitializer::OnPermissionsUpdated, WrapPersistent(this))));

  return promise;
}

void MIDIAccessInitializer::DidAddInputPort(const String& id,
                                            const String& manufacturer,
                                            const String& name,
                                            const String& version,
                                            PortState state) {
  DCHECK(accessor_);
  port_descriptors_.push_back(PortDescriptor(
      id, manufacturer, name, MIDIPort::kTypeInput, version, state));
}

void MIDIAccessInitializer::DidAddOutputPort(const String& id,
                                             const String& manufacturer,
                                             const String& name,
                                             const String& version,
                                             PortState state) {
  DCHECK(accessor_);
  port_descriptors_.push_back(PortDescriptor(
      id, manufacturer, name, MIDIPort::kTypeOutput, version, state));
}

void MIDIAccessInitializer::DidSetInputPortState(unsigned port_index,
                                                 PortState state) {
  // didSetInputPortState() is not allowed to call before didStartSession()
  // is called. Once didStartSession() is called, MIDIAccessorClient methods
  // are delegated to MIDIAccess. See constructor of MIDIAccess.
  NOTREACHED();
}

void MIDIAccessInitializer::DidSetOutputPortState(unsigned port_index,
                                                  PortState state) {
  // See comments on didSetInputPortState().
  NOTREACHED();
}

void MIDIAccessInitializer::DidStartSession(Result result) {
  DCHECK(accessor_);
  // We would also have AbortError and SecurityError according to the spec.
  // SecurityError is handled in onPermission(s)Updated().
  switch (result) {
    case Result::NOT_INITIALIZED:
      break;
    case Result::OK:
      return Resolve(MIDIAccess::Create(
          std::move(accessor_), options_.hasSysex() && options_.sysex(),
          port_descriptors_, GetExecutionContext()));
    case Result::NOT_SUPPORTED:
      return Reject(DOMException::Create(kNotSupportedError));
    case Result::INITIALIZATION_ERROR:
      return Reject(DOMException::Create(
          kInvalidStateError, "Platform dependent initialization failed."));
  }
  NOTREACHED();
  Reject(DOMException::Create(kInvalidStateError,
                              "Unknown internal error occurred."));
}

ExecutionContext* MIDIAccessInitializer::GetExecutionContext() const {
  return ExecutionContext::From(GetScriptState());
}

void MIDIAccessInitializer::OnPermissionsUpdated(PermissionStatus status) {
  permission_service_.reset();
  if (status == PermissionStatus::GRANTED)
    accessor_->StartSession();
  else
    Reject(DOMException::Create(kSecurityError));
}

void MIDIAccessInitializer::OnPermissionUpdated(PermissionStatus status) {
  permission_service_.reset();
  if (status == PermissionStatus::GRANTED)
    accessor_->StartSession();
  else
    Reject(DOMException::Create(kSecurityError));
}

}  // namespace blink
