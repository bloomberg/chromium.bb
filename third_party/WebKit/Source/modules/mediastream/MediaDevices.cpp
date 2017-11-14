// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/mediastream/MediaDevices.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/events/Event.h"
#include "modules/mediastream/MediaDevicesRequest.h"
#include "modules/mediastream/MediaErrorState.h"
#include "modules/mediastream/MediaStream.h"
#include "modules/mediastream/MediaStreamConstraints.h"
#include "modules/mediastream/NavigatorMediaStream.h"
#include "modules/mediastream/NavigatorUserMediaErrorCallback.h"
#include "modules/mediastream/NavigatorUserMediaSuccessCallback.h"
#include "modules/mediastream/UserMediaController.h"
#include "modules/mediastream/UserMediaRequest.h"
#include "platform/bindings/ScriptState.h"

namespace blink {

namespace {

class PromiseSuccessCallback final : public NavigatorUserMediaSuccessCallback {
 public:
  explicit PromiseSuccessCallback(ScriptPromiseResolver* resolver)
      : resolver_(resolver) {}

  ~PromiseSuccessCallback() {}

  void handleEvent(MediaStream* stream) { resolver_->Resolve(stream); }

  virtual void Trace(blink::Visitor* visitor) {
    visitor->Trace(resolver_);
    NavigatorUserMediaSuccessCallback::Trace(visitor);
  }

 private:
  Member<ScriptPromiseResolver> resolver_;
};

class PromiseErrorCallback final : public NavigatorUserMediaErrorCallback {
 public:
  explicit PromiseErrorCallback(ScriptPromiseResolver* resolver)
      : resolver_(resolver) {}

  ~PromiseErrorCallback() {}

  void handleEvent(DOMExceptionOrOverconstrainedError error) {
    resolver_->Reject(error);
  }

  virtual void Trace(blink::Visitor* visitor) {
    visitor->Trace(resolver_);
    NavigatorUserMediaErrorCallback::Trace(visitor);
  }

 private:
  Member<ScriptPromiseResolver> resolver_;
};

}  // namespace

MediaDevices* MediaDevices::Create(ExecutionContext* context) {
  MediaDevices* media_devices = new MediaDevices(context);
  media_devices->PauseIfNeeded();
  return media_devices;
}

MediaDevices::MediaDevices(ExecutionContext* context)
    : PausableObject(context),
      observing_(false),
      stopped_(false),
      dispatch_scheduled_event_runner_(AsyncMethodRunner<MediaDevices>::Create(
          this,
          &MediaDevices::DispatchScheduledEvent)) {}

MediaDevices::~MediaDevices() {}

ScriptPromise MediaDevices::enumerateDevices(ScriptState* script_state) {
  Document* document = ToDocument(ExecutionContext::From(script_state));
  UserMediaController* user_media =
      UserMediaController::From(document->GetFrame());
  if (!user_media)
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(kNotSupportedError,
                             "No media device controller available; is this a "
                             "detached window?"));

  MediaDevicesRequest* request =
      MediaDevicesRequest::Create(script_state, user_media);
  return request->Start();
}

ScriptPromise MediaDevices::getUserMedia(ScriptState* script_state,
                                         const MediaStreamConstraints& options,
                                         ExceptionState& exception_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);

  NavigatorUserMediaSuccessCallback* success_callback =
      new PromiseSuccessCallback(resolver);
  NavigatorUserMediaErrorCallback* error_callback =
      new PromiseErrorCallback(resolver);

  Document* document = ToDocument(ExecutionContext::From(script_state));
  UserMediaController* user_media =
      UserMediaController::From(document->GetFrame());
  if (!user_media)
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(kNotSupportedError,
                             "No media device controller available; is this a "
                             "detached window?"));

  MediaErrorState error_state;
  UserMediaRequest* request =
      UserMediaRequest::Create(document, user_media, options, success_callback,
                               error_callback, error_state);
  if (!request) {
    DCHECK(error_state.HadException());
    if (error_state.CanGenerateException()) {
      error_state.RaiseException(exception_state);
      return exception_state.Reject(script_state);
    }
    ScriptPromise rejected_promise = resolver->Promise();
    resolver->Reject(error_state.CreateError());
    return rejected_promise;
  }

  String error_message;
  if (!request->IsSecureContextUse(error_message)) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(kNotSupportedError, error_message));
  }
  auto promise = resolver->Promise();
  request->Start();
  return promise;
}

void MediaDevices::DidChangeMediaDevices() {
  Document* document = ToDocument(GetExecutionContext());
  DCHECK(document);

  if (RuntimeEnabledFeatures::OnDeviceChangeEnabled())
    ScheduleDispatchEvent(Event::Create(EventTypeNames::devicechange));
}

const AtomicString& MediaDevices::InterfaceName() const {
  return EventTargetNames::MediaDevices;
}

ExecutionContext* MediaDevices::GetExecutionContext() const {
  return PausableObject::GetExecutionContext();
}

void MediaDevices::RemoveAllEventListeners() {
  EventTargetWithInlineData::RemoveAllEventListeners();
  DCHECK(!HasEventListeners());
  StopObserving();
}

void MediaDevices::AddedEventListener(
    const AtomicString& event_type,
    RegisteredEventListener& registered_listener) {
  EventTargetWithInlineData::AddedEventListener(event_type,
                                                registered_listener);
  StartObserving();
}

void MediaDevices::RemovedEventListener(
    const AtomicString& event_type,
    const RegisteredEventListener& registered_listener) {
  EventTargetWithInlineData::RemovedEventListener(event_type,
                                                  registered_listener);
  if (!HasEventListeners())
    StopObserving();
}

bool MediaDevices::HasPendingActivity() const {
  DCHECK(stopped_ || observing_ == HasEventListeners());
  return observing_;
}

void MediaDevices::ContextDestroyed(ExecutionContext*) {
  if (stopped_)
    return;

  stopped_ = true;
  StopObserving();
}

void MediaDevices::Pause() {
  dispatch_scheduled_event_runner_->Pause();
}

void MediaDevices::Unpause() {
  dispatch_scheduled_event_runner_->Unpause();
}

void MediaDevices::ScheduleDispatchEvent(Event* event) {
  scheduled_events_.push_back(event);
  dispatch_scheduled_event_runner_->RunAsync();
}

void MediaDevices::DispatchScheduledEvent() {
  HeapVector<Member<Event>> events;
  events.swap(scheduled_events_);

  for (const auto& event : events)
    DispatchEvent(event);
}

void MediaDevices::StartObserving() {
  if (observing_ || stopped_)
    return;

  UserMediaController* user_media = GetUserMediaController();
  if (!user_media)
    return;

  user_media->SetMediaDeviceChangeObserver(this);
  observing_ = true;
}

void MediaDevices::StopObserving() {
  if (!observing_)
    return;

  UserMediaController* user_media = GetUserMediaController();
  if (!user_media)
    return;

  user_media->SetMediaDeviceChangeObserver(nullptr);
  observing_ = false;
}

UserMediaController* MediaDevices::GetUserMediaController() {
  Document* document = ToDocument(GetExecutionContext());
  if (!document)
    return nullptr;

  return UserMediaController::From(document->GetFrame());
}

void MediaDevices::Dispose() {
  StopObserving();
}

void MediaDevices::Trace(blink::Visitor* visitor) {
  visitor->Trace(dispatch_scheduled_event_runner_);
  visitor->Trace(scheduled_events_);
  EventTargetWithInlineData::Trace(visitor);
  PausableObject::Trace(visitor);
}

}  // namespace blink
