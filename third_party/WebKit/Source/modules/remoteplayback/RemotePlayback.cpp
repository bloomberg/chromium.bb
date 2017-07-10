// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/remoteplayback/RemotePlayback.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/modules/v8/RemotePlaybackAvailabilityCallback.h"
#include "core/HTMLNames.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/dom/UserGestureIndicator.h"
#include "core/events/Event.h"
#include "core/html/HTMLMediaElement.h"
#include "core/html/HTMLVideoElement.h"
#include "core/probe/CoreProbes.h"
#include "modules/EventTargetModules.h"
#include "modules/presentation/PresentationController.h"
#include "modules/remoteplayback/AvailabilityCallbackWrapper.h"
#include "platform/MemoryCoordinator.h"
#include "platform/json/JSONValues.h"
#include "platform/wtf/text/Base64.h"
#include "public/platform/modules/presentation/WebPresentationClient.h"

namespace blink {

namespace {

const AtomicString& RemotePlaybackStateToString(WebRemotePlaybackState state) {
  DEFINE_STATIC_LOCAL(const AtomicString, connecting_value, ("connecting"));
  DEFINE_STATIC_LOCAL(const AtomicString, connected_value, ("connected"));
  DEFINE_STATIC_LOCAL(const AtomicString, disconnected_value, ("disconnected"));

  switch (state) {
    case WebRemotePlaybackState::kConnecting:
      return connecting_value;
    case WebRemotePlaybackState::kConnected:
      return connected_value;
    case WebRemotePlaybackState::kDisconnected:
      return disconnected_value;
  }

  NOTREACHED();
  return disconnected_value;
}

void RunNotifyInitialAvailabilityTask(ExecutionContext* context,
                                      std::unique_ptr<WTF::Closure> task) {
  probe::AsyncTask async_task(context, task.get());
  (*task)();
}

WebURL GetAvailabilityUrl(const WebURL& source) {
  if (source.IsEmpty() || !source.IsValid())
    return WebURL();

  // The URL for each media element's source looks like the following:
  // chrome-media-source://<encoded-data> where |encoded-data| is base64 URL
  // encoded string representation of a JSON structure with various information
  // about the media element's source that looks like this:
  // {
  //   "sourceUrl": "<source url>",
  // }
  // TODO(avayvod): add and fill more info to the JSON structure, like the
  // frame URL, audio/video codec info, the result of the CORS check, etc.
  std::unique_ptr<JSONObject> source_info = JSONObject::Create();
  source_info->SetString("sourceUrl", source.GetString());
  CString json_source_info = source_info->ToJSONString().Utf8();
  String encoded_source_info =
      WTF::Base64URLEncode(json_source_info.data(), json_source_info.length());

  return KURL(kParsedURLString, "remote-playback://" + encoded_source_info);
}

bool IsBackgroundAvailabilityMonitoringDisabled() {
  return MemoryCoordinator::IsLowEndDevice();
}

}  // anonymous namespace

// static
RemotePlayback* RemotePlayback::Create(HTMLMediaElement& element) {
  return new RemotePlayback(element);
}

RemotePlayback::RemotePlayback(HTMLMediaElement& element)
    : state_(element.IsPlayingRemotely()
                 ? WebRemotePlaybackState::kConnected
                 : WebRemotePlaybackState::kDisconnected),
      availability_(WebRemotePlaybackAvailability::kUnknown),
      media_element_(&element),
      is_listening_(false) {}

const AtomicString& RemotePlayback::InterfaceName() const {
  return EventTargetNames::RemotePlayback;
}

ExecutionContext* RemotePlayback::GetExecutionContext() const {
  return &media_element_->GetDocument();
}

ScriptPromise RemotePlayback::watchAvailability(
    ScriptState* script_state,
    RemotePlaybackAvailabilityCallback* callback) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  if (media_element_->FastHasAttribute(HTMLNames::disableremoteplaybackAttr)) {
    resolver->Reject(DOMException::Create(
        kInvalidStateError, "disableRemotePlayback attribute is present."));
    return promise;
  }

  int id = WatchAvailabilityInternal(new AvailabilityCallbackWrapper(callback));
  if (id == kWatchAvailabilityNotSupported) {
    resolver->Reject(DOMException::Create(
        kNotSupportedError,
        "Availability monitoring is not supported on this device."));
    return promise;
  }

  // TODO(avayvod): Currently the availability is tracked for each media element
  // as soon as it's created, we probably want to limit that to when the
  // page/element is visible (see https://crbug.com/597281) and has default
  // controls. If there are no default controls, we should also start tracking
  // availability on demand meaning the Promise returned by watchAvailability()
  // will be resolved asynchronously.
  resolver->Resolve(id);
  return promise;
}

ScriptPromise RemotePlayback::cancelWatchAvailability(ScriptState* script_state,
                                                      int id) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  if (media_element_->FastHasAttribute(HTMLNames::disableremoteplaybackAttr)) {
    resolver->Reject(DOMException::Create(
        kInvalidStateError, "disableRemotePlayback attribute is present."));
    return promise;
  }

  if (!CancelWatchAvailabilityInternal(id)) {
    resolver->Reject(DOMException::Create(
        kNotFoundError, "A callback with the given id is not found."));
    return promise;
  }

  resolver->Resolve();
  return promise;
}

ScriptPromise RemotePlayback::cancelWatchAvailability(
    ScriptState* script_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  if (media_element_->FastHasAttribute(HTMLNames::disableremoteplaybackAttr)) {
    resolver->Reject(DOMException::Create(
        kInvalidStateError, "disableRemotePlayback attribute is present."));
    return promise;
  }

  availability_callbacks_.clear();
  StopListeningForAvailability();

  resolver->Resolve();
  return promise;
}

ScriptPromise RemotePlayback::prompt(ScriptState* script_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  if (media_element_->FastHasAttribute(HTMLNames::disableremoteplaybackAttr)) {
    resolver->Reject(DOMException::Create(
        kInvalidStateError, "disableRemotePlayback attribute is present."));
    return promise;
  }

  if (prompt_promise_resolver_) {
    resolver->Reject(DOMException::Create(
        kOperationError,
        "A prompt is already being shown for this media element."));
    return promise;
  }

  if (!UserGestureIndicator::ProcessingUserGesture()) {
    resolver->Reject(DOMException::Create(
        kInvalidAccessError,
        "RemotePlayback::prompt() requires user gesture."));
    return promise;
  }

  if (!RuntimeEnabledFeatures::RemotePlaybackBackendEnabled()) {
    resolver->Reject(DOMException::Create(
        kNotSupportedError,
        "The RemotePlayback API is disabled on this platform."));
    return promise;
  }

  if (availability_ == WebRemotePlaybackAvailability::kDeviceNotAvailable) {
    resolver->Reject(DOMException::Create(kNotFoundError,
                                          "No remote playback devices found."));
    return promise;
  }

  if (availability_ == WebRemotePlaybackAvailability::kSourceNotCompatible) {
    resolver->Reject(DOMException::Create(
        kNotSupportedError,
        "The currentSrc is not compatible with remote playback"));
    return promise;
  }

  prompt_promise_resolver_ = resolver;
  PromptInternal();

  return promise;
}

String RemotePlayback::state() const {
  return RemotePlaybackStateToString(state_);
}

bool RemotePlayback::HasPendingActivity() const {
  return HasEventListeners() || !availability_callbacks_.IsEmpty() ||
         prompt_promise_resolver_;
}

void RemotePlayback::PromptInternal() {
  if (state_ == WebRemotePlaybackState::kDisconnected)
    media_element_->RequestRemotePlayback();
  else
    media_element_->RequestRemotePlaybackControl();
}

int RemotePlayback::WatchAvailabilityInternal(
    AvailabilityCallbackWrapper* callback) {
  if (RuntimeEnabledFeatures::RemotePlaybackBackendEnabled() &&
      IsBackgroundAvailabilityMonitoringDisabled()) {
    return kWatchAvailabilityNotSupported;
  }

  int id;
  do {
    id = GetExecutionContext()->CircularSequentialID();
  } while (!availability_callbacks_
                .insert(id, TraceWrapperMember<AvailabilityCallbackWrapper>(
                                this, callback))
                .is_new_entry);

  // Report the current availability via the callback.
  // TODO(yuryu): Wrapping notifyInitialAvailability with WTF::Closure as
  // InspectorInstrumentation requires a globally unique pointer to track tasks.
  // We can remove the wrapper if InspectorInstrumentation returns a task id.
  std::unique_ptr<WTF::Closure> task = WTF::Bind(
      &RemotePlayback::NotifyInitialAvailability, WrapPersistent(this), id);
  probe::AsyncTaskScheduled(GetExecutionContext(), "watchAvailabilityCallback",
                            task.get());
  TaskRunnerHelper::Get(TaskType::kMediaElementEvent, GetExecutionContext())
      ->PostTask(BLINK_FROM_HERE,
                 WTF::Bind(RunNotifyInitialAvailabilityTask,
                           WrapPersistent(GetExecutionContext()),
                           WTF::Passed(std::move(task))));

  MaybeStartListeningForAvailability();
  return id;
}

bool RemotePlayback::CancelWatchAvailabilityInternal(int id) {
  auto iter = availability_callbacks_.find(id);
  if (iter == availability_callbacks_.end())
    return false;
  availability_callbacks_.erase(iter);
  if (availability_callbacks_.IsEmpty())
    StopListeningForAvailability();

  return true;
}

void RemotePlayback::NotifyInitialAvailability(int callback_id) {
  // May not find the callback if the website cancels it fast enough.
  auto iter = availability_callbacks_.find(callback_id);
  if (iter == availability_callbacks_.end())
    return;

  iter->value->Run(this, RemotePlaybackAvailable());
}

void RemotePlayback::StateChanged(WebRemotePlaybackState state) {
  if (state_ == state)
    return;

  if (prompt_promise_resolver_) {
    // Changing state to Disconnected from "disconnected" or "connecting" means
    // that establishing connection with remote playback device failed.
    // Changing state to anything else means the state change intended by
    // prompt() succeeded.
    if (state_ != WebRemotePlaybackState::kConnected &&
        state == WebRemotePlaybackState::kDisconnected) {
      prompt_promise_resolver_->Reject(DOMException::Create(
          kAbortError, "Failed to connect to the remote device."));
    } else {
      DCHECK((state_ == WebRemotePlaybackState::kDisconnected &&
              state == WebRemotePlaybackState::kConnecting) ||
             (state_ == WebRemotePlaybackState::kConnected &&
              state == WebRemotePlaybackState::kDisconnected));
      prompt_promise_resolver_->Resolve();
    }
    prompt_promise_resolver_ = nullptr;
  }

  state_ = state;
  switch (state_) {
    case WebRemotePlaybackState::kConnecting:
      DispatchEvent(Event::Create(EventTypeNames::connecting));
      if (RuntimeEnabledFeatures::NewRemotePlaybackPipelineEnabled() &&
          media_element_->IsHTMLVideoElement()) {
        toHTMLVideoElement(media_element_)->MediaRemotingStarted();
      }
      break;
    case WebRemotePlaybackState::kConnected:
      DispatchEvent(Event::Create(EventTypeNames::connect));
      break;
    case WebRemotePlaybackState::kDisconnected:
      DispatchEvent(Event::Create(EventTypeNames::disconnect));
      if (RuntimeEnabledFeatures::NewRemotePlaybackPipelineEnabled() &&
          media_element_->IsHTMLVideoElement()) {
        toHTMLVideoElement(media_element_)->MediaRemotingStopped();
      }
      break;
  }
}

void RemotePlayback::AvailabilityChanged(
    WebRemotePlaybackAvailability availability) {
  if (availability_ == availability)
    return;

  bool old_availability = RemotePlaybackAvailable();
  availability_ = availability;
  bool new_availability = RemotePlaybackAvailable();
  if (new_availability == old_availability)
    return;

  for (auto& callback : availability_callbacks_.Values())
    callback->Run(this, new_availability);
}

void RemotePlayback::PromptCancelled() {
  if (!prompt_promise_resolver_)
    return;

  prompt_promise_resolver_->Reject(
      DOMException::Create(kNotAllowedError, "The prompt was dismissed."));
  prompt_promise_resolver_ = nullptr;
}

void RemotePlayback::SourceChanged(const WebURL& source) {
  DCHECK(RuntimeEnabledFeatures::NewRemotePlaybackPipelineEnabled());

  if (IsBackgroundAvailabilityMonitoringDisabled())
    return;

  WebURL current_url =
      availability_urls_.IsEmpty() ? WebURL() : availability_urls_[0];
  WebURL new_url = GetAvailabilityUrl(source);

  if (new_url == current_url)
    return;

  // Tell PresentationController to stop listening for availability before the
  // URLs vector is updated.
  StopListeningForAvailability();

  // WebVector doesn't have push_back or alternative.
  if (new_url.IsEmpty()) {
    WebVector<WebURL> empty;
    availability_urls_.Swap(empty);
  } else {
    WebVector<WebURL> new_urls((size_t)1);
    new_urls[0] = new_url;
    availability_urls_.Swap(new_urls);
  }

  MaybeStartListeningForAvailability();
}

bool RemotePlayback::RemotePlaybackAvailable() const {
  if (IsBackgroundAvailabilityMonitoringDisabled() &&
      RuntimeEnabledFeatures::RemotePlaybackBackendEnabled() &&
      !media_element_->currentSrc().IsEmpty()) {
    return true;
  }

  return availability_ == WebRemotePlaybackAvailability::kDeviceAvailable;
}

void RemotePlayback::RemotePlaybackDisabled() {
  if (prompt_promise_resolver_) {
    prompt_promise_resolver_->Reject(DOMException::Create(
        kInvalidStateError, "disableRemotePlayback attribute is present."));
    prompt_promise_resolver_ = nullptr;
  }

  availability_callbacks_.clear();
  StopListeningForAvailability();

  if (state_ != WebRemotePlaybackState::kDisconnected)
    media_element_->RequestRemotePlaybackStop();
}

void RemotePlayback::AvailabilityChanged(
    mojom::ScreenAvailability availability) {
  DCHECK(RuntimeEnabledFeatures::NewRemotePlaybackPipelineEnabled());
  DCHECK(is_listening_);

  // TODO(avayvod): Use mojom::ScreenAvailability directly once
  // WebRemotePlaybackAvailability is gone with the old pipeline.
  WebRemotePlaybackAvailability remote_playback_availability =
      WebRemotePlaybackAvailability::kUnknown;
  switch (availability) {
    case mojom::ScreenAvailability::UNKNOWN:
    case mojom::ScreenAvailability::DISABLED:
      NOTREACHED();
      remote_playback_availability = WebRemotePlaybackAvailability::kUnknown;
      break;
    case mojom::ScreenAvailability::UNAVAILABLE:
      remote_playback_availability =
          WebRemotePlaybackAvailability::kDeviceNotAvailable;
      break;
    case mojom::ScreenAvailability::SOURCE_NOT_SUPPORTED:
      remote_playback_availability =
          WebRemotePlaybackAvailability::kSourceNotCompatible;
      break;
    case mojom::ScreenAvailability::AVAILABLE:
      remote_playback_availability =
          WebRemotePlaybackAvailability::kDeviceAvailable;
      break;
  }
  AvailabilityChanged(remote_playback_availability);
}

const WebVector<WebURL>& RemotePlayback::Urls() const {
  DCHECK(RuntimeEnabledFeatures::NewRemotePlaybackPipelineEnabled());
  // TODO(avayvod): update the URL format and add frame url, mime type and
  // response headers when available.
  return availability_urls_;
}

void RemotePlayback::StopListeningForAvailability() {
  if (!RuntimeEnabledFeatures::RemotePlaybackBackendEnabled())
    return;

  if (!is_listening_)
    return;

  availability_ = WebRemotePlaybackAvailability::kUnknown;
  WebPresentationClient* client =
      PresentationController::ClientFromContext(GetExecutionContext());
  if (!client)
    return;

  client->StopListening(this);
  is_listening_ = false;
}

void RemotePlayback::MaybeStartListeningForAvailability() {
  if (IsBackgroundAvailabilityMonitoringDisabled())
    return;

  if (!RuntimeEnabledFeatures::RemotePlaybackBackendEnabled())
    return;

  if (is_listening_)
    return;

  if (availability_urls_.empty() || availability_callbacks_.IsEmpty())
    return;

  WebPresentationClient* client =
      PresentationController::ClientFromContext(GetExecutionContext());
  if (!client)
    return;

  client->StartListening(this);
  is_listening_ = true;
}

DEFINE_TRACE(RemotePlayback) {
  visitor->Trace(availability_callbacks_);
  visitor->Trace(prompt_promise_resolver_);
  visitor->Trace(media_element_);
  EventTargetWithInlineData::Trace(visitor);
}

DEFINE_TRACE_WRAPPERS(RemotePlayback) {
  for (auto callback : availability_callbacks_.Values())
    visitor->TraceWrappers(callback);
  EventTargetWithInlineData::TraceWrappers(visitor);
}

}  // namespace blink
