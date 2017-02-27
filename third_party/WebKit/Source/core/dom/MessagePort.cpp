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
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/SerializedScriptValue.h"
#include "bindings/core/v8/SerializedScriptValueFactory.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/ExecutionContextTask.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/events/MessageEvent.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/workers/WorkerGlobalScope.h"
#include "public/platform/WebString.h"
#include "wtf/Atomics.h"
#include "wtf/PtrUtil.h"
#include "wtf/text/AtomicString.h"

namespace blink {

MessagePort* MessagePort::create(ExecutionContext& executionContext) {
  return new MessagePort(executionContext);
}

MessagePort::MessagePort(ExecutionContext& executionContext)
    : ContextLifecycleObserver(&executionContext),
      m_pendingDispatchTask(0),
      m_started(false),
      m_closed(false) {}

MessagePort::~MessagePort() {
  DCHECK(!m_started || !isEntangled());
}

void MessagePort::postMessage(ScriptState* scriptState,
                              PassRefPtr<SerializedScriptValue> message,
                              const MessagePortArray& ports,
                              ExceptionState& exceptionState) {
  if (!isEntangled())
    return;
  DCHECK(getExecutionContext());
  DCHECK(m_entangledChannel);

  // Make sure we aren't connected to any of the passed-in ports.
  for (unsigned i = 0; i < ports.size(); ++i) {
    if (ports[i] == this) {
      exceptionState.throwDOMException(
          DataCloneError,
          "Port at index " + String::number(i) + " contains the source port.");
      return;
    }
  }
  MessagePortChannelArray channels =
      MessagePort::disentanglePorts(scriptState->getExecutionContext(), ports,
                                    exceptionState);
  if (exceptionState.hadException())
    return;

  WebString messageString = message->toWireString();
  WebMessagePortChannelArray webChannels =
      toWebMessagePortChannelArray(std::move(channels));
  m_entangledChannel->postMessage(messageString, std::move(webChannels));
}

// static
WebMessagePortChannelArray MessagePort::toWebMessagePortChannelArray(
    MessagePortChannelArray channels) {
  WebMessagePortChannelArray webChannels(channels.size());
  for (size_t i = 0; i < channels.size(); ++i)
    webChannels[i] = std::move(channels[i]);
  return webChannels;
}

// static
MessagePortArray* MessagePort::toMessagePortArray(
    ExecutionContext* context,
    WebMessagePortChannelArray webChannels) {
  MessagePortChannelArray channels(webChannels.size());
  for (size_t i = 0; i < webChannels.size(); ++i)
    channels[i] = std::move(webChannels[i]);
  return MessagePort::entanglePorts(*context, std::move(channels));
}

WebMessagePortChannelUniquePtr MessagePort::disentangle() {
  DCHECK(m_entangledChannel);
  m_entangledChannel->setClient(nullptr);
  return std::move(m_entangledChannel);
}

// Invoked to notify us that there are messages available for this port.
// This code may be called from another thread, and so should not call any
// non-threadsafe APIs (i.e. should not call into the entangled channel or
// access mutable variables).
void MessagePort::messageAvailable() {
  // Don't post another task if there's an identical one pending.
  if (atomicTestAndSetToOne(&m_pendingDispatchTask))
    return;

  DCHECK(getExecutionContext());
  // TODO(tzik): Use ParentThreadTaskRunners instead of ExecutionContext here to
  // avoid touching foreign thread GCed object.
  getExecutionContext()->postTask(
      TaskType::PostedMessage, BLINK_FROM_HERE,
      createCrossThreadTask(&MessagePort::dispatchMessages,
                            wrapCrossThreadWeakPersistent(this)));
}

void MessagePort::start() {
  // Do nothing if we've been cloned or closed.
  if (!isEntangled())
    return;

  DCHECK(getExecutionContext());
  if (m_started)
    return;

  m_entangledChannel->setClient(this);
  m_started = true;
  messageAvailable();
}

void MessagePort::close() {
  if (isEntangled())
    m_entangledChannel->setClient(nullptr);
  m_closed = true;
}

void MessagePort::entangle(WebMessagePortChannelUniquePtr remote) {
  // Only invoked to set our initial entanglement.
  DCHECK(!m_entangledChannel);
  DCHECK(getExecutionContext());

  m_entangledChannel = std::move(remote);
}

const AtomicString& MessagePort::interfaceName() const {
  return EventTargetNames::MessagePort;
}

static bool tryGetMessageFrom(WebMessagePortChannel& webChannel,
                              RefPtr<SerializedScriptValue>& message,
                              MessagePortChannelArray& channels) {
  WebString messageString;
  WebMessagePortChannelArray webChannels;
  if (!webChannel.tryGetMessage(&messageString, webChannels))
    return false;

  if (webChannels.size()) {
    channels.resize(webChannels.size());
    for (size_t i = 0; i < webChannels.size(); ++i)
      channels[i] = std::move(webChannels[i]);
  }
  message = SerializedScriptValue::create(messageString);
  return true;
}

bool MessagePort::tryGetMessage(RefPtr<SerializedScriptValue>& message,
                                MessagePortChannelArray& channels) {
  if (!m_entangledChannel)
    return false;
  return tryGetMessageFrom(*m_entangledChannel, message, channels);
}

void MessagePort::dispatchMessages() {
  // Signal to |messageAvailable()| that there are no ongoing
  // dispatches of messages. This can cause redundantly posted
  // tasks, but safely avoids messages languishing.
  releaseStore(&m_pendingDispatchTask, 0);

  // Messages for contexts that are not fully active get dispatched too, but
  // JSAbstractEventListener::handleEvent() doesn't call handlers for these.
  // The HTML5 spec specifies that any messages sent to a document that is not
  // fully active should be dropped, so this behavior is OK.
  if (!started())
    return;

  while (true) {
    // Because close() doesn't cancel any in flight calls to dispatchMessages(),
    // and can be triggered by the onmessage event handler, we need to check if
    // the port is still open before each dispatch.
    if (m_closed)
      break;

    // WorkerGlobalScope::close() in Worker onmessage handler should prevent
    // the next message from dispatching.
    if (getExecutionContext()->isWorkerGlobalScope() &&
        toWorkerGlobalScope(getExecutionContext())->isClosing()) {
      break;
    }

    RefPtr<SerializedScriptValue> message;
    MessagePortChannelArray channels;
    if (!tryGetMessage(message, channels))
      break;

    MessagePortArray* ports =
        MessagePort::entanglePorts(*getExecutionContext(), std::move(channels));
    Event* evt = MessageEvent::create(ports, std::move(message));

    dispatchEvent(evt);
  }
}

bool MessagePort::hasPendingActivity() const {
  // The spec says that entangled message ports should always be treated as if
  // they have a strong reference.
  // We'll also stipulate that the queue needs to be open (if the app drops its
  // reference to the port before start()-ing it, then it's not really entangled
  // as it's unreachable).
  return m_started && isEntangled();
}

MessagePortChannelArray MessagePort::disentanglePorts(
    ExecutionContext* context,
    const MessagePortArray& ports,
    ExceptionState& exceptionState) {
  if (!ports.size())
    return MessagePortChannelArray();

  HeapHashSet<Member<MessagePort>> visited;

  // Walk the incoming array - if there are any duplicate ports, or null ports
  // or cloned ports, throw an error (per section 8.3.3 of the HTML5 spec).
  for (unsigned i = 0; i < ports.size(); ++i) {
    MessagePort* port = ports[i];
    if (!port || port->isNeutered() || visited.contains(port)) {
      String type;
      if (!port)
        type = "null";
      else if (port->isNeutered())
        type = "already neutered";
      else
        type = "a duplicate";
      exceptionState.throwDOMException(
          DataCloneError,
          "Port at index " + String::number(i) + " is " + type + ".");
      return MessagePortChannelArray();
    }
    visited.insert(port);
  }

  UseCounter::count(context, UseCounter::MessagePortsTransferred);

  // Passed-in ports passed validity checks, so we can disentangle them.
  MessagePortChannelArray portArray(ports.size());
  for (unsigned i = 0; i < ports.size(); ++i)
    portArray[i] = ports[i]->disentangle();
  return portArray;
}

MessagePortArray* MessagePort::entanglePorts(ExecutionContext& context,
                                             MessagePortChannelArray channels) {
  // https://html.spec.whatwg.org/multipage/comms.html#message-ports
  // |ports| should be an empty array, not null even when there is no ports.
  MessagePortArray* portArray = new MessagePortArray(channels.size());
  for (unsigned i = 0; i < channels.size(); ++i) {
    MessagePort* port = MessagePort::create(context);
    port->entangle(std::move(channels[i]));
    (*portArray)[i] = port;
  }
  return portArray;
}

DEFINE_TRACE(MessagePort) {
  ContextLifecycleObserver::trace(visitor);
  EventTargetWithInlineData::trace(visitor);
}

}  // namespace blink
