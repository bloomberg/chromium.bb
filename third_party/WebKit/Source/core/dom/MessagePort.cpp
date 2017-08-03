/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "core/dom/MessagePort.h"

#include <memory>
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/serialization/SerializedScriptValue.h"
#include "bindings/core/v8/serialization/SerializedScriptValueFactory.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/events/MessageEvent.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/workers/WorkerGlobalScope.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/bindings/ScriptState.h"
#include "platform/wtf/Atomics.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/text/AtomicString.h"
#include "public/platform/WebString.h"

namespace blink {

MessagePort* MessagePort::Create(ExecutionContext& execution_context) {
  return new MessagePort(execution_context);
}

MessagePort::MessagePort(ExecutionContext& execution_context)
    : ContextLifecycleObserver(&execution_context),
      task_runner_(TaskRunnerHelper::Get(TaskType::kPostedMessage,
                                         &execution_context)) {}

MessagePort::~MessagePort() {
  DCHECK(!started_ || !IsEntangled());
}

void MessagePort::postMessage(ScriptState* script_state,
                              RefPtr<SerializedScriptValue> message,
                              const MessagePortArray& ports,
                              ExceptionState& exception_state) {
  if (!IsEntangled())
    return;
  DCHECK(GetExecutionContext());
  DCHECK(entangled_channel_);

  // Make sure we aren't connected to any of the passed-in ports.
  for (unsigned i = 0; i < ports.size(); ++i) {
    if (ports[i] == this) {
      exception_state.ThrowDOMException(
          kDataCloneError,
          "Port at index " + String::Number(i) + " contains the source port.");
      return;
    }
  }
  MessagePortChannelArray channels = MessagePort::DisentanglePorts(
      ExecutionContext::From(script_state), ports, exception_state);
  if (exception_state.HadException())
    return;

  StringView wire_data = message->GetWireData();
  WebMessagePortChannelArray web_channels =
      ToWebMessagePortChannelArray(std::move(channels));
  entangled_channel_->PostMessage(
      reinterpret_cast<const uint8_t*>(wire_data.Characters8()),
      wire_data.length(), std::move(web_channels));
}

// static
WebMessagePortChannelArray MessagePort::ToWebMessagePortChannelArray(
    MessagePortChannelArray channels) {
  WebMessagePortChannelArray web_channels(channels.size());
  for (size_t i = 0; i < channels.size(); ++i)
    web_channels[i] = std::move(channels[i]);
  return web_channels;
}

// static
MessagePortArray* MessagePort::ToMessagePortArray(
    ExecutionContext* context,
    WebMessagePortChannelArray web_channels) {
  MessagePortChannelArray channels(web_channels.size());
  for (size_t i = 0; i < web_channels.size(); ++i)
    channels[i] = std::move(web_channels[i]);
  return MessagePort::EntanglePorts(*context, std::move(channels));
}

std::unique_ptr<WebMessagePortChannel> MessagePort::Disentangle() {
  DCHECK(entangled_channel_);
  entangled_channel_->SetClient(nullptr);
  return std::move(entangled_channel_);
}

// Invoked to notify us that there are messages available for this port.
// This code may be called from another thread, and so should not call any
// non-threadsafe APIs (i.e. should not call into the entangled channel or
// access mutable variables).
void MessagePort::MessageAvailable() {
  // Don't post another task if there's an identical one pending.
  if (AtomicTestAndSetToOne(&pending_dispatch_task_))
    return;

  task_runner_->PostTask(BLINK_FROM_HERE,
                         CrossThreadBind(&MessagePort::DispatchMessages,
                                         WrapCrossThreadWeakPersistent(this)));
}

void MessagePort::start() {
  // Do nothing if we've been cloned or closed.
  if (!IsEntangled())
    return;

  DCHECK(GetExecutionContext());
  if (started_)
    return;

  entangled_channel_->SetClient(this);
  started_ = true;
  MessageAvailable();
}

void MessagePort::close() {
  if (IsEntangled())
    entangled_channel_->SetClient(nullptr);
  closed_ = true;
}

void MessagePort::Entangle(std::unique_ptr<WebMessagePortChannel> remote) {
  // Only invoked to set our initial entanglement.
  DCHECK(!entangled_channel_);
  DCHECK(GetExecutionContext());

  entangled_channel_ = std::move(remote);
}

const AtomicString& MessagePort::InterfaceName() const {
  return EventTargetNames::MessagePort;
}

static bool TryGetMessageFrom(WebMessagePortChannel& web_channel,
                              RefPtr<SerializedScriptValue>& message,
                              MessagePortChannelArray& channels) {
  WebVector<uint8_t> message_data;
  WebMessagePortChannelArray web_channels;
  if (!web_channel.TryGetMessage(&message_data, web_channels))
    return false;

  if (web_channels.size()) {
    channels.resize(web_channels.size());
    for (size_t i = 0; i < web_channels.size(); ++i)
      channels[i] = std::move(web_channels[i]);
  }
  message = SerializedScriptValue::Create(
      reinterpret_cast<const char*>(message_data.Data()), message_data.size());
  return true;
}

bool MessagePort::TryGetMessage(RefPtr<SerializedScriptValue>& message,
                                MessagePortChannelArray& channels) {
  if (!entangled_channel_)
    return false;
  return TryGetMessageFrom(*entangled_channel_, message, channels);
}

void MessagePort::DispatchMessages() {
  // Signal to |messageAvailable()| that there are no ongoing
  // dispatches of messages. This can cause redundantly posted
  // tasks, but safely avoids messages languishing.
  ReleaseStore(&pending_dispatch_task_, 0);

  // Messages for contexts that are not fully active get dispatched too, but
  // JSAbstractEventListener::handleEvent() doesn't call handlers for these.
  // The HTML5 spec specifies that any messages sent to a document that is not
  // fully active should be dropped, so this behavior is OK.
  if (!Started())
    return;

  while (true) {
    // Because close() doesn't cancel any in flight calls to dispatchMessages(),
    // and can be triggered by the onmessage event handler, we need to check if
    // the port is still open before each dispatch.
    if (closed_)
      break;

    // WorkerGlobalScope::close() in Worker onmessage handler should prevent
    // the next message from dispatching.
    if (GetExecutionContext()->IsWorkerGlobalScope() &&
        ToWorkerGlobalScope(GetExecutionContext())->IsClosing()) {
      break;
    }

    RefPtr<SerializedScriptValue> message;
    MessagePortChannelArray channels;
    if (!TryGetMessage(message, channels))
      break;

    MessagePortArray* ports =
        MessagePort::EntanglePorts(*GetExecutionContext(), std::move(channels));
    Event* evt = MessageEvent::Create(ports, std::move(message));

    DispatchEvent(evt);
  }
}

bool MessagePort::HasPendingActivity() const {
  // The spec says that entangled message ports should always be treated as if
  // they have a strong reference.
  // We'll also stipulate that the queue needs to be open (if the app drops its
  // reference to the port before start()-ing it, then it's not really entangled
  // as it's unreachable).
  return started_ && IsEntangled();
}

MessagePortChannelArray MessagePort::DisentanglePorts(
    ExecutionContext* context,
    const MessagePortArray& ports,
    ExceptionState& exception_state) {
  if (!ports.size())
    return MessagePortChannelArray();

  HeapHashSet<Member<MessagePort>> visited;

  // Walk the incoming array - if there are any duplicate ports, or null ports
  // or cloned ports, throw an error (per section 8.3.3 of the HTML5 spec).
  for (unsigned i = 0; i < ports.size(); ++i) {
    MessagePort* port = ports[i];
    if (!port || port->IsNeutered() || visited.Contains(port)) {
      String type;
      if (!port)
        type = "null";
      else if (port->IsNeutered())
        type = "already neutered";
      else
        type = "a duplicate";
      exception_state.ThrowDOMException(
          kDataCloneError,
          "Port at index " + String::Number(i) + " is " + type + ".");
      return MessagePortChannelArray();
    }
    visited.insert(port);
  }

  UseCounter::Count(context, WebFeature::kMessagePortsTransferred);

  // Passed-in ports passed validity checks, so we can disentangle them.
  MessagePortChannelArray port_array(ports.size());
  for (unsigned i = 0; i < ports.size(); ++i)
    port_array[i] = ports[i]->Disentangle();
  return port_array;
}

MessagePortArray* MessagePort::EntanglePorts(ExecutionContext& context,
                                             MessagePortChannelArray channels) {
  // https://html.spec.whatwg.org/multipage/comms.html#message-ports
  // |ports| should be an empty array, not null even when there is no ports.
  MessagePortArray* port_array = new MessagePortArray(channels.size());
  for (unsigned i = 0; i < channels.size(); ++i) {
    MessagePort* port = MessagePort::Create(context);
    port->Entangle(std::move(channels[i]));
    (*port_array)[i] = port;
  }
  return port_array;
}

DEFINE_TRACE(MessagePort) {
  ContextLifecycleObserver::Trace(visitor);
  EventTargetWithInlineData::Trace(visitor);
}

}  // namespace blink
