/*
 * Copyright (C) 2011 Google Inc.  All rights reserved.
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

#ifndef DOMWebSocket_h
#define DOMWebSocket_h

#include <stddef.h>
#include <stdint.h>
#include <memory>
#include "core/dom/ArrayBufferViewHelpers.h"
#include "core/dom/SuspendableObject.h"
#include "core/events/EventListener.h"
#include "core/events/EventTarget.h"
#include "modules/EventTargetModules.h"
#include "modules/ModulesExport.h"
#include "modules/websockets/WebSocketChannel.h"
#include "modules/websockets/WebSocketChannelClient.h"
#include "platform/Timer.h"
#include "platform/bindings/ActiveScriptWrappable.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/Deque.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/PassRefPtr.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class Blob;
class DOMArrayBuffer;
class DOMArrayBufferView;
class ExceptionState;
class ExecutionContext;
class StringOrStringSequence;

class MODULES_EXPORT DOMWebSocket : public EventTargetWithInlineData,
                                    public ActiveScriptWrappable<DOMWebSocket>,
                                    public SuspendableObject,
                                    public WebSocketChannelClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(DOMWebSocket);

 public:
  static const char* SubprotocolSeperator();
  // DOMWebSocket instances must be used with a wrapper since this class's
  // lifetime management is designed assuming the V8 holds a ref on it while
  // hasPendingActivity() returns true.
  static DOMWebSocket* Create(ExecutionContext*,
                              const String& url,
                              ExceptionState&);
  static DOMWebSocket* Create(ExecutionContext*,
                              const String& url,
                              const StringOrStringSequence& protocols,
                              ExceptionState&);
  ~DOMWebSocket() override;

  enum State { kConnecting = 0, kOpen = 1, kClosing = 2, kClosed = 3 };

  void Connect(const String& url,
               const Vector<String>& protocols,
               ExceptionState&);

  void send(const String& message, ExceptionState&);
  void send(DOMArrayBuffer*, ExceptionState&);
  void send(NotShared<DOMArrayBufferView>, ExceptionState&);
  void send(Blob*, ExceptionState&);

  // To distinguish close method call with the code parameter from one
  // without, we have these three signatures. Use of
  // Optional=DefaultIsUndefined in the IDL file doesn't help for now since
  // it's bound to a value of 0 which is indistinguishable from the case 0
  // is passed as code parameter.
  void close(unsigned short code, const String& reason, ExceptionState&);
  void close(ExceptionState&);
  void close(unsigned short code, ExceptionState&);

  const KURL& url() const;
  State readyState() const;
  unsigned bufferedAmount() const;

  String protocol() const;
  String extensions() const;

  String binaryType() const;
  void setBinaryType(const String&);

  DEFINE_ATTRIBUTE_EVENT_LISTENER(open);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(message);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(error);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(close);

  // EventTarget functions.
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  // SuspendableObject functions.
  void ContextDestroyed(ExecutionContext*) override;
  void Suspend() override;
  void Resume() override;

  // ScriptWrappable functions.
  // Prevent this instance from being collected while it's not in CLOSED
  // state.
  bool HasPendingActivity() const final;

  // WebSocketChannelClient functions.
  void DidConnect(const String& subprotocol, const String& extensions) override;
  void DidReceiveTextMessage(const String& message) override;
  void DidReceiveBinaryMessage(std::unique_ptr<Vector<char>>) override;
  void DidError() override;
  void DidConsumeBufferedAmount(uint64_t) override;
  void DidStartClosingHandshake() override;
  void DidClose(ClosingHandshakeCompletionStatus,
                unsigned short code,
                const String& reason) override;

  DECLARE_VIRTUAL_TRACE();

  static bool IsValidSubprotocolString(const String&);

 protected:
  explicit DOMWebSocket(ExecutionContext*);

 private:
  // FIXME: This should inherit blink::EventQueue.
  class EventQueue final : public GarbageCollectedFinalized<EventQueue> {
   public:
    static EventQueue* Create(EventTarget* target) {
      return new EventQueue(target);
    }
    ~EventQueue();

    // Dispatches the event if this queue is active.
    // Queues the event if this queue is suspended.
    // Does nothing otherwise.
    void Dispatch(Event* /* event */);

    bool IsEmpty() const;

    void Suspend();
    void Resume();
    void ContextDestroyed();

    DECLARE_TRACE();

   private:
    enum State {
      kActive,
      kSuspended,
      kStopped,
    };

    explicit EventQueue(EventTarget*);

    // Dispatches queued events if this queue is active.
    // Does nothing otherwise.
    void DispatchQueuedEvents();
    void ResumeTimerFired(TimerBase*);

    State state_;
    Member<EventTarget> target_;
    HeapDeque<Member<Event>> events_;
    TaskRunnerTimer<EventQueue> resume_timer_;
  };

  enum WebSocketSendType {
    kWebSocketSendTypeString,
    kWebSocketSendTypeArrayBuffer,
    kWebSocketSendTypeArrayBufferView,
    kWebSocketSendTypeBlob,
    kWebSocketSendTypeMax,
  };

  enum WebSocketReceiveType {
    kWebSocketReceiveTypeString,
    kWebSocketReceiveTypeArrayBuffer,
    kWebSocketReceiveTypeBlob,
    kWebSocketReceiveTypeMax,
  };

  enum BinaryType { kBinaryTypeBlob, kBinaryTypeArrayBuffer };

  // This function is virtual for unittests.
  // FIXME: Move WebSocketChannel::create here.
  virtual WebSocketChannel* CreateChannel(ExecutionContext* context,
                                          WebSocketChannelClient* client) {
    return WebSocketChannel::Create(context, client);
  }

  // Adds a console message with JSMessageSource and ErrorMessageLevel.
  void LogError(const String& message);

  // Handle the JavaScript close method call. close() methods on this class
  // are just for determining if the optional code argument is supplied or
  // not.
  void CloseInternal(int, const String&, ExceptionState&);

  // Updates m_bufferedAmountAfterClose given the amount of data passed to
  // send() method after the state changed to CLOSING or CLOSED.
  void UpdateBufferedAmountAfterClose(uint64_t);
  void ReflectBufferedAmountConsumption(TimerBase*);

  void ReleaseChannel();
  void RecordSendTypeHistogram(WebSocketSendType);
  void RecordSendMessageSizeHistogram(WebSocketSendType, size_t);
  void RecordReceiveTypeHistogram(WebSocketReceiveType);
  void RecordReceiveMessageSizeHistogram(WebSocketReceiveType, size_t);

  Member<WebSocketChannel> channel_;

  State state_;
  KURL url_;
  uint64_t buffered_amount_;
  // The consumed buffered amount that will be reflected to m_bufferedAmount
  // later. It will be cleared once reflected.
  uint64_t consumed_buffered_amount_;
  uint64_t buffered_amount_after_close_;
  BinaryType binary_type_;
  // The subprotocol the server selected.
  String subprotocol_;
  String extensions_;

  Member<EventQueue> event_queue_;
  TaskRunnerTimer<DOMWebSocket> buffered_amount_consume_timer_;
};

}  // namespace blink

#endif  // DOMWebSocket_h
