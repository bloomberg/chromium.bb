/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef RTCDataChannel_h
#define RTCDataChannel_h

#include <memory>
#include "base/gtest_prod_util.h"
#include "core/dom/SuspendableObject.h"
#include "core/typed_arrays/ArrayBufferViewHelpers.h"
#include "modules/EventTargetModules.h"
#include "platform/Timer.h"
#include "platform/bindings/ActiveScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Compiler.h"
#include "public/platform/WebRTCDataChannelHandler.h"
#include "public/platform/WebRTCDataChannelHandlerClient.h"

namespace blink {

class Blob;
class DOMArrayBuffer;
class DOMArrayBufferView;
class ExceptionState;
class WebRTCDataChannelHandler;
class WebRTCPeerConnectionHandler;
struct WebRTCDataChannelInit;

class MODULES_EXPORT RTCDataChannel final
    : public EventTargetWithInlineData,
      public WebRTCDataChannelHandlerClient,
      public ActiveScriptWrappable<RTCDataChannel>,
      public SuspendableObject {
  USING_GARBAGE_COLLECTED_MIXIN(RTCDataChannel);
  DEFINE_WRAPPERTYPEINFO();
  USING_PRE_FINALIZER(RTCDataChannel, Dispose);

 public:
  static RTCDataChannel* Create(ExecutionContext*,
                                std::unique_ptr<WebRTCDataChannelHandler>);
  static RTCDataChannel* Create(ExecutionContext*,
                                WebRTCPeerConnectionHandler*,
                                const String& label,
                                const WebRTCDataChannelInit&,
                                ExceptionState&);
  ~RTCDataChannel() override;

  ReadyState GetHandlerState() const;

  String label() const;

  // DEPRECATED
  bool reliable() const;

  bool ordered() const;
  unsigned short maxRetransmitTime() const;
  unsigned short maxRetransmits() const;
  String protocol() const;
  bool negotiated() const;
  unsigned short id() const;
  String readyState() const;
  unsigned bufferedAmount() const;

  unsigned bufferedAmountLowThreshold() const;
  void setBufferedAmountLowThreshold(unsigned);

  String binaryType() const;
  void setBinaryType(const String&, ExceptionState&);

  void send(const String&, ExceptionState&);
  void send(DOMArrayBuffer*, ExceptionState&);
  void send(NotShared<DOMArrayBufferView>, ExceptionState&);
  void send(Blob*, ExceptionState&);

  void close();

  DEFINE_ATTRIBUTE_EVENT_LISTENER(open);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(bufferedamountlow);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(error);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(close);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(message);

  // EventTarget
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  // SuspendableObject
  void Suspend() override;
  void Resume() override;
  void ContextDestroyed(ExecutionContext*) override;

  // ScriptWrappable
  bool HasPendingActivity() const override;

  DECLARE_VIRTUAL_TRACE();

  // WebRTCDataChannelHandlerClient
  void DidChangeReadyState(WebRTCDataChannelHandlerClient::ReadyState) override;
  void DidDecreaseBufferedAmount(unsigned) override;
  void DidReceiveStringData(const WebString&) override;
  void DidReceiveRawData(const char*, size_t) override;
  void DidDetectError() override;

 private:
  RTCDataChannel(ExecutionContext*, std::unique_ptr<WebRTCDataChannelHandler>);
  void Dispose();

  void ScheduleDispatchEvent(Event*);
  void ScheduledEventTimerFired(TimerBase*);

  std::unique_ptr<WebRTCDataChannelHandler> handler_;

  WebRTCDataChannelHandlerClient::ReadyState ready_state_;

  enum BinaryType { kBinaryTypeBlob, kBinaryTypeArrayBuffer };
  BinaryType binary_type_;

  TaskRunnerTimer<RTCDataChannel> scheduled_event_timer_;
  HeapVector<Member<Event>> scheduled_events_;

  unsigned buffered_amount_low_threshold_;

  bool stopped_;

  FRIEND_TEST_ALL_PREFIXES(RTCDataChannelTest, BufferedAmountLow);
};

}  // namespace blink

#endif  // RTCDataChannel_h
