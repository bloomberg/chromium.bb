// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/serial/serial.h"

#include <inttypes.h>

#include "base/unguessable_token.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/event_target_modules_names.h"
#include "third_party/blink/renderer/modules/serial/serial_port.h"

namespace blink {

namespace {

const char kNoPortSelected[] = "No port selected by the user.";

String TokenToString(const base::UnguessableToken& token) {
  // TODO(crbug.com/918702): Implement HashTraits for UnguessableToken.
  return String::Format("%016" PRIX64 "%016" PRIX64,
                        token.GetHighForSerialization(),
                        token.GetLowForSerialization());
}

}  // namespace

// static
Serial* Serial::Create(ExecutionContext& execution_context) {
  return MakeGarbageCollected<Serial>(execution_context);
}

Serial::Serial(ExecutionContext& execution_context)
    : ContextLifecycleObserver(&execution_context) {}

ExecutionContext* Serial::GetExecutionContext() const {
  return ContextLifecycleObserver::GetExecutionContext();
}

const AtomicString& Serial::InterfaceName() const {
  return event_target_names::kSerial;
}

ScriptPromise Serial::getPorts(ScriptState* script_state) {
  if (!GetExecutionContext()) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(DOMExceptionCode::kNotSupportedError));
  }

  auto* resolver = ScriptPromiseResolver::Create(script_state);
  get_ports_promises_.insert(resolver);

  EnsureServiceConnection();
  service_->GetPorts(WTF::Bind(&Serial::OnGetPorts, WrapPersistent(this),
                               WrapPersistent(resolver)));

  return resolver->Promise();
}

ScriptPromise Serial::requestPort(ScriptState* script_state,
                                  const SerialPortRequestOptions* options) {
  auto* frame = GetFrame();
  if (!frame) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(DOMExceptionCode::kNotSupportedError));
  }

  if (!LocalFrame::HasTransientUserActivation(frame)) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(
            DOMExceptionCode::kSecurityError,
            "Must be handling a user gesture to show a permission request."));
  }

  auto* resolver = ScriptPromiseResolver::Create(script_state);
  request_port_promises_.insert(resolver);

  EnsureServiceConnection();
  service_->RequestPort(Vector<mojom::blink::SerialPortFilterPtr>(),
                        WTF::Bind(&Serial::OnRequestPort, WrapPersistent(this),
                                  WrapPersistent(resolver)));

  return resolver->Promise();
}

void Serial::Trace(Visitor* visitor) {
  visitor->Trace(get_ports_promises_);
  visitor->Trace(request_port_promises_);
  visitor->Trace(port_cache_);
  EventTargetWithInlineData::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

void Serial::EnsureServiceConnection() {
  DCHECK(GetExecutionContext());

  if (service_)
    return;

  auto task_runner =
      GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI);
  GetExecutionContext()->GetInterfaceProvider()->GetInterface(
      mojo::MakeRequest(&service_, task_runner));
  service_.set_connection_error_handler(
      WTF::Bind(&Serial::OnServiceConnectionError, WrapWeakPersistent(this)));
}

void Serial::OnServiceConnectionError() {
  service_.reset();

  // Script may execute during a call to Resolve(). Swap these sets to prevent
  // concurrent modification.
  HeapHashSet<Member<ScriptPromiseResolver>> get_ports_promises;
  get_ports_promises_.swap(get_ports_promises);
  for (ScriptPromiseResolver* resolver : get_ports_promises)
    resolver->Resolve(HeapVector<Member<SerialPort>>());

  HeapHashSet<Member<ScriptPromiseResolver>> request_port_promises;
  request_port_promises_.swap(request_port_promises);
  for (ScriptPromiseResolver* resolver : request_port_promises) {
    resolver->Reject(DOMException::Create(DOMExceptionCode::kNotFoundError,
                                          kNoPortSelected));
  }
}

SerialPort* Serial::GetOrCreatePort(mojom::blink::SerialPortInfoPtr info) {
  SerialPort* port = port_cache_.at(TokenToString(info->token));
  if (!port) {
    port = MakeGarbageCollected<SerialPort>(std::move(info));
    port_cache_.insert(TokenToString(port->Token()), port);
  }
  return port;
}

void Serial::OnGetPorts(ScriptPromiseResolver* resolver,
                        Vector<mojom::blink::SerialPortInfoPtr> port_infos) {
  DCHECK(get_ports_promises_.Contains(resolver));
  get_ports_promises_.erase(resolver);

  HeapVector<Member<SerialPort>> ports;
  for (auto& port_info : port_infos)
    ports.push_back(GetOrCreatePort(std::move(port_info)));

  resolver->Resolve(ports);
}

void Serial::OnRequestPort(ScriptPromiseResolver* resolver,
                           mojom::blink::SerialPortInfoPtr port_info) {
  DCHECK(request_port_promises_.Contains(resolver));
  request_port_promises_.erase(resolver);

  if (!port_info) {
    resolver->Reject(DOMException::Create(DOMExceptionCode::kNotFoundError,
                                          kNoPortSelected));
    return;
  }

  resolver->Resolve(GetOrCreatePort(std::move(port_info)));
}

}  // namespace blink
