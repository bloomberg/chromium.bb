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

#ifndef MessagePort_h
#define MessagePort_h

#include <memory>
#include "bindings/core/v8/serialization/SerializedScriptValue.h"
#include "core/CoreExport.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/events/EventListener.h"
#include "core/dom/events/EventTarget.h"
#include "platform/WebTaskRunner.h"
#include "platform/bindings/ActiveScriptWrappable.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/Vector.h"
#include "third_party/WebKit/common/message_port/message_port_channel.h"

namespace blink {

struct BlinkMessagePortMessage;
class ExceptionState;
class ExecutionContext;
class ScriptState;
class SerializedScriptValue;

class CORE_EXPORT MessagePort : public EventTargetWithInlineData,
                                public ActiveScriptWrappable<MessagePort>,
                                public ContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(MessagePort);

 public:
  static MessagePort* Create(ExecutionContext&);
  ~MessagePort() override;

  void postMessage(ScriptState*,
                   RefPtr<SerializedScriptValue> message,
                   const MessagePortArray&,
                   ExceptionState&);
  static bool CanTransferArrayBuffersAndImageBitmaps() { return false; }

  void start();
  void close();

  void Entangle(mojo::ScopedMessagePipeHandle);
  void Entangle(MessagePortChannel);
  MessagePortChannel Disentangle();

  // Returns an empty array if there is an exception, or if the passed array is
  // empty.
  static Vector<MessagePortChannel> DisentanglePorts(ExecutionContext*,
                                                     const MessagePortArray&,
                                                     ExceptionState&);

  // Returns an empty array if the passed array is empty.
  static MessagePortArray* EntanglePorts(ExecutionContext&,
                                         Vector<MessagePortChannel>);
  static MessagePortArray* EntanglePorts(ExecutionContext&,
                                         WebVector<MessagePortChannel>);

  bool Started() const { return started_; }

  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override {
    return ContextLifecycleObserver::GetExecutionContext();
  }
  MessagePort* ToMessagePort() override { return this; }

  // ScriptWrappable implementation.
  bool HasPendingActivity() const final;

  // ContextLifecycleObserver implementation.
  void ContextDestroyed(ExecutionContext*) override { close(); }

  void setOnmessage(EventListener* listener) {
    SetAttributeEventListener(EventTypeNames::message, listener);
    start();
  }
  EventListener* onmessage() {
    return GetAttributeEventListener(EventTypeNames::message);
  }

  void setOnmessageerror(EventListener* listener) {
    SetAttributeEventListener(EventTypeNames::messageerror, listener);
    start();
  }
  EventListener* onmessageerror() {
    return GetAttributeEventListener(EventTypeNames::messageerror);
  }

  // A port starts out its life entangled, and remains entangled until it is
  // closed or is cloned.
  bool IsEntangled() const { return !closed_ && !IsNeutered(); }

  // A port gets neutered when it is transferred to a new owner via
  // postMessage().
  bool IsNeutered() const { return !channel_.GetHandle().is_valid(); }

  // For testing only: allows inspection of the entangled channel.
  MojoHandle EntangledHandleForTesting() const;

  DECLARE_VIRTUAL_TRACE();

 protected:
  explicit MessagePort(ExecutionContext&);
  bool TryGetMessage(BlinkMessagePortMessage&);

 private:
  void MessageAvailable();
  void DispatchMessages();

  MessagePortChannel channel_;

  int pending_dispatch_task_ = 0;
  bool started_ = false;
  bool closed_ = false;

  RefPtr<WebTaskRunner> task_runner_;
};

}  // namespace blink

#endif  // MessagePort_h
