/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef RTCDTMFSender_h
#define RTCDTMFSender_h

#include "core/dom/ContextLifecycleObserver.h"
#include "modules/EventTargetModules.h"
#include "platform/Timer.h"
#include "public/platform/WebRTCDTMFSenderHandlerClient.h"
#include <memory>

namespace blink {

class ExceptionState;
class MediaStreamTrack;
class WebRTCDTMFSenderHandler;
class WebRTCPeerConnectionHandler;

class RTCDTMFSender final : public EventTargetWithInlineData,
                            public WebRTCDTMFSenderHandlerClient,
                            public ContextLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(RTCDTMFSender);
  DEFINE_WRAPPERTYPEINFO();
  USING_PRE_FINALIZER(RTCDTMFSender, Dispose);

 public:
  static RTCDTMFSender* Create(ExecutionContext*,
                               WebRTCPeerConnectionHandler*,
                               MediaStreamTrack*,
                               ExceptionState&);
  ~RTCDTMFSender() override;

  bool canInsertDTMF() const;
  MediaStreamTrack* track() const;
  String toneBuffer() const;
  int duration() const { return duration_; }
  int interToneGap() const { return inter_tone_gap_; }

  void insertDTMF(const String& tones, ExceptionState&);
  void insertDTMF(const String& tones, int duration, ExceptionState&);
  void insertDTMF(const String& tones,
                  int duration,
                  int inter_tone_gap,
                  ExceptionState&);

  DEFINE_ATTRIBUTE_EVENT_LISTENER(tonechange);

  // EventTarget
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  // ContextLifecycleObserver
  void ContextDestroyed(ExecutionContext*) override;

  virtual void Trace(blink::Visitor*);

 private:
  RTCDTMFSender(ExecutionContext*,
                MediaStreamTrack*,
                std::unique_ptr<WebRTCDTMFSenderHandler>);
  void Dispose();

  void ScheduleDispatchEvent(Event*);
  void ScheduledEventTimerFired(TimerBase*);

  // WebRTCDTMFSenderHandlerClient
  void DidPlayTone(const WebString&) override;

  Member<MediaStreamTrack> track_;
  int duration_;
  int inter_tone_gap_;

  std::unique_ptr<WebRTCDTMFSenderHandler> handler_;

  bool stopped_;

  TaskRunnerTimer<RTCDTMFSender> scheduled_event_timer_;
  HeapVector<Member<Event>> scheduled_events_;
};

}  // namespace blink

#endif  // RTCDTMFSender_h
