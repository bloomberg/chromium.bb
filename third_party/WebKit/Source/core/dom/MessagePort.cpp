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
#include "core/dom/BlinkTransferableMessageStructTraits.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/events/MessageEvent.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/UseCounter.h"
#include "core/workers/WorkerGlobalScope.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/bindings/ScriptState.h"
#include "platform/wtf/Atomics.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/text/AtomicString.h"
#include "public/platform/WebString.h"

namespace blink {

// The maximum number of MessageEvents to dispatch from one task.
static const int kMaximumMessagesPerTask = 200;

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
  DCHECK(!IsNeutered());

  BlinkTransferableMessage msg;
  msg.message = message;

  // Make sure we aren't connected to any of the passed-in ports.
  for (unsigned i = 0; i < ports.size(); ++i) {
    if (ports[i] == this) {
      exception_state.ThrowDOMException(
          kDataCloneError,
          "Port at index " + String::Number(i) + " contains the source port.");
      return;
    }
  }
  msg.ports = MessagePort::DisentanglePorts(
      ExecutionContext::From(script_state), ports, exception_state);
  if (exception_state.HadException())
    return;

  channel_.PostMojoMessage(
      mojom::blink::TransferableMessage::SerializeAsMessage(&msg));
}

MessagePortChannel MessagePort::Disentangle() {
  DCHECK(!IsNeutered());
  channel_.ClearCallback();
  auto result = std::move(channel_);
  channel_ = MessagePortChannel();
  return result;
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
                                         WrapCrossThreadPersistent(this)));
}

void MessagePort::start() {
  // Do nothing if we've been cloned or closed.
  if (!IsEntangled())
    return;

  DCHECK(GetExecutionContext());
  if (started_)
    return;

  // Note that MessagePortChannel may call this callback on any thread.
  channel_.SetCallback(ConvertToBaseCallback(CrossThreadBind(
      &MessagePort::MessageAvailable, WrapCrossThreadWeakPersistent(this))));
  started_ = true;
  MessageAvailable();
}

void MessagePort::close() {
  // A closed port should not be neutered, so rather than merely disconnecting
  // from the mojo message pipe, also entangle with a new dangling message pipe.
  channel_.ClearCallback();
  if (IsEntangled())
    channel_ = MessagePortChannel(mojo::MessagePipe().handle0);
  closed_ = true;
}

void MessagePort::Entangle(mojo::ScopedMessagePipeHandle handle) {
  Entangle(MessagePortChannel(std::move(handle)));
}

void MessagePort::Entangle(MessagePortChannel channel) {
  // Only invoked to set our initial entanglement.
  DCHECK(!channel_.GetHandle().is_valid());
  DCHECK(channel.GetHandle().is_valid());
  DCHECK(GetExecutionContext());

  channel_ = std::move(channel);
}

const AtomicString& MessagePort::InterfaceName() const {
  return EventTargetNames::MessagePort;
}

bool MessagePort::TryGetMessage(BlinkTransferableMessage& message) {
  if (IsNeutered())
    return false;

  mojo::Message mojo_message;
  if (!channel_.GetMojoMessage(&mojo_message))
    return false;

  if (!mojom::blink::TransferableMessage::DeserializeFromMessage(
          std::move(mojo_message), &message)) {
    return false;
  }

  return true;
}

void MessagePort::DispatchMessages() {
  // Signal to |MessageAvailable()| that there are no ongoing
  // dispatches of messages. This can cause redundantly posted
  // tasks, but safely avoids messages languishing.
  ReleaseStore(&pending_dispatch_task_, 0);

  // Messages for contexts that are not fully active get dispatched too, but
  // JSAbstractEventListener::handleEvent() doesn't call handlers for these.
  // The HTML5 spec specifies that any messages sent to a document that is not
  // fully active should be dropped, so this behavior is OK.
  if (!Started())
    return;

  // There's an upper bound on the loop iterations in one DispatchMessages call,
  // otherwise page JS could make it loop forever or starve other work.
  for (int i = 0; i < kMaximumMessagesPerTask; ++i) {
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

    BlinkTransferableMessage message;
    if (!TryGetMessage(message))
      break;

    MessagePortArray* ports = MessagePort::EntanglePorts(
        *GetExecutionContext(), std::move(message.ports));
    Event* evt = MessageEvent::Create(ports, std::move(message.message));

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

Vector<MessagePortChannel> MessagePort::DisentanglePorts(
    ExecutionContext* context,
    const MessagePortArray& ports,
    ExceptionState& exception_state) {
  if (!ports.size())
    return Vector<MessagePortChannel>();

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
      return Vector<MessagePortChannel>();
    }
    visited.insert(port);
  }

  UseCounter::Count(context, WebFeature::kMessagePortsTransferred);

  // Passed-in ports passed validity checks, so we can disentangle them.
  Vector<MessagePortChannel> channels;
  channels.ReserveInitialCapacity(ports.size());
  for (unsigned i = 0; i < ports.size(); ++i)
    channels.push_back(ports[i]->Disentangle());
  return channels;
}

MessagePortArray* MessagePort::EntanglePorts(
    ExecutionContext& context,
    Vector<MessagePortChannel> channels) {
  return EntanglePorts(context,
                       WebVector<MessagePortChannel>(std::move(channels)));
}

MessagePortArray* MessagePort::EntanglePorts(
    ExecutionContext& context,
    WebVector<MessagePortChannel> channels) {
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

MojoHandle MessagePort::EntangledHandleForTesting() const {
  return channel_.GetHandle().get().value();
}

void MessagePort::Trace(blink::Visitor* visitor) {
  ContextLifecycleObserver::Trace(visitor);
  EventTargetWithInlineData::Trace(visitor);
}

}  // namespace blink
