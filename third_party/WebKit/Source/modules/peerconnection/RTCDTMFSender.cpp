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

#include "modules/peerconnection/RTCDTMFSender.h"

#include <memory>
#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/TaskRunnerHelper.h"
#include "modules/mediastream/MediaStreamTrack.h"
#include "modules/peerconnection/RTCDTMFToneChangeEvent.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/WebMediaStreamTrack.h"
#include "public/platform/WebRTCDTMFSenderHandler.h"
#include "public/platform/WebRTCPeerConnectionHandler.h"

namespace blink {

static const int kMinToneDurationMs = 70;
static const int kDefaultToneDurationMs = 100;
static const int kMaxToneDurationMs = 6000;
static const int kMinInterToneGapMs = 50;
static const int kDefaultInterToneGapMs = 50;

RTCDTMFSender* RTCDTMFSender::Create(
    ExecutionContext* context,
    WebRTCPeerConnectionHandler* peer_connection_handler,
    MediaStreamTrack* track,
    ExceptionState& exception_state) {
  std::unique_ptr<WebRTCDTMFSenderHandler> handler = WTF::WrapUnique(
      peer_connection_handler->CreateDTMFSender(track->Component()));
  if (!handler) {
    exception_state.ThrowDOMException(kNotSupportedError,
                                      "The MediaStreamTrack provided is not an "
                                      "element of a MediaStream that's "
                                      "currently in the local streams set.");
    return nullptr;
  }

  return new RTCDTMFSender(context, track, std::move(handler));
}

RTCDTMFSender::RTCDTMFSender(ExecutionContext* context,
                             MediaStreamTrack* track,
                             std::unique_ptr<WebRTCDTMFSenderHandler> handler)
    : ContextLifecycleObserver(context),
      track_(track),
      duration_(kDefaultToneDurationMs),
      inter_tone_gap_(kDefaultInterToneGapMs),
      handler_(std::move(handler)),
      stopped_(false),
      scheduled_event_timer_(
          TaskRunnerHelper::Get(TaskType::kNetworking, context),
          this,
          &RTCDTMFSender::ScheduledEventTimerFired) {
  handler_->SetClient(this);
}

RTCDTMFSender::~RTCDTMFSender() {}

void RTCDTMFSender::Dispose() {
  // Promptly clears a raw reference from content/ to an on-heap object
  // so that content/ doesn't access it in a lazy sweeping phase.
  handler_->SetClient(nullptr);
  handler_.reset();
}

bool RTCDTMFSender::canInsertDTMF() const {
  return handler_->CanInsertDTMF();
}

MediaStreamTrack* RTCDTMFSender::track() const {
  return track_.Get();
}

String RTCDTMFSender::toneBuffer() const {
  return handler_->CurrentToneBuffer();
}

void RTCDTMFSender::insertDTMF(const String& tones,
                               ExceptionState& exception_state) {
  insertDTMF(tones, kDefaultToneDurationMs, kDefaultInterToneGapMs,
             exception_state);
}

void RTCDTMFSender::insertDTMF(const String& tones,
                               int duration,
                               ExceptionState& exception_state) {
  insertDTMF(tones, duration, kDefaultInterToneGapMs, exception_state);
}

void RTCDTMFSender::insertDTMF(const String& tones,
                               int duration,
                               int inter_tone_gap,
                               ExceptionState& exception_state) {
  if (!canInsertDTMF()) {
    exception_state.ThrowDOMException(kNotSupportedError,
                                      "The 'canInsertDTMF' attribute is false: "
                                      "this sender cannot send DTMF.");
    return;
  }

  if (duration > kMaxToneDurationMs || duration < kMinToneDurationMs) {
    exception_state.ThrowDOMException(
        kSyntaxError,
        ExceptionMessages::IndexOutsideRange(
            "duration", duration, kMinToneDurationMs,
            ExceptionMessages::kExclusiveBound, kMaxToneDurationMs,
            ExceptionMessages::kExclusiveBound));
    return;
  }

  if (inter_tone_gap < kMinInterToneGapMs) {
    exception_state.ThrowDOMException(
        kSyntaxError, ExceptionMessages::IndexExceedsMinimumBound(
                          "intertone gap", inter_tone_gap, kMinInterToneGapMs));
    return;
  }

  duration_ = duration;
  inter_tone_gap_ = inter_tone_gap;

  if (!handler_->InsertDTMF(tones, duration_, inter_tone_gap_))
    exception_state.ThrowDOMException(
        kSyntaxError, "Could not send provided tones, '" + tones + "'.");
}

void RTCDTMFSender::DidPlayTone(const WebString& tone) {
  ScheduleDispatchEvent(RTCDTMFToneChangeEvent::Create(tone));
}

const AtomicString& RTCDTMFSender::InterfaceName() const {
  return EventTargetNames::RTCDTMFSender;
}

ExecutionContext* RTCDTMFSender::GetExecutionContext() const {
  return ContextLifecycleObserver::GetExecutionContext();
}

void RTCDTMFSender::ContextDestroyed(ExecutionContext*) {
  stopped_ = true;
  handler_->SetClient(nullptr);
}

void RTCDTMFSender::ScheduleDispatchEvent(Event* event) {
  scheduled_events_.push_back(event);

  if (!scheduled_event_timer_.IsActive())
    scheduled_event_timer_.StartOneShot(0, BLINK_FROM_HERE);
}

void RTCDTMFSender::ScheduledEventTimerFired(TimerBase*) {
  if (stopped_)
    return;

  HeapVector<Member<Event>> events;
  events.swap(scheduled_events_);

  HeapVector<Member<Event>>::iterator it = events.begin();
  for (; it != events.end(); ++it)
    DispatchEvent((*it).Release());
}

void RTCDTMFSender::Trace(blink::Visitor* visitor) {
  visitor->Trace(track_);
  visitor->Trace(scheduled_events_);
  EventTargetWithInlineData::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
