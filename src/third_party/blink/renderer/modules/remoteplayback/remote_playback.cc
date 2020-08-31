// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/remoteplayback/remote_playback.h"

#include <memory>
#include <utility>

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_remote_playback_availability_callback.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/html/media/remote_playback_observer.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/probe/async_task_id.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/presentation/presentation_availability_state.h"
#include "third_party/blink/renderer/modules/presentation/presentation_controller.h"
#include "third_party/blink/renderer/modules/remoteplayback/availability_callback_wrapper.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/memory_pressure_listener.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"

namespace blink {

namespace {

const AtomicString& RemotePlaybackStateToString(
    mojom::blink::PresentationConnectionState state) {
  DEFINE_STATIC_LOCAL(const AtomicString, connecting_value, ("connecting"));
  DEFINE_STATIC_LOCAL(const AtomicString, connected_value, ("connected"));
  DEFINE_STATIC_LOCAL(const AtomicString, disconnected_value, ("disconnected"));

  switch (state) {
    case mojom::blink::PresentationConnectionState::CONNECTING:
      return connecting_value;
    case mojom::blink::PresentationConnectionState::CONNECTED:
      return connected_value;
    case mojom::blink::PresentationConnectionState::CLOSED:
    case mojom::blink::PresentationConnectionState::TERMINATED:
      return disconnected_value;
    default:
      NOTREACHED();
      return disconnected_value;
  }
}

void RunRemotePlaybackTask(ExecutionContext* context,
                           base::OnceClosure task,
                           std::unique_ptr<probe::AsyncTaskId> task_id) {
  probe::AsyncTask async_task(context, task_id.get());
  std::move(task).Run();
}

KURL GetAvailabilityUrl(const WebURL& source, bool is_source_supported) {
  if (source.IsEmpty() || !source.IsValid() || !is_source_supported)
    return KURL();

  // The URL for each media element's source looks like the following:
  // remote-playback://<encoded-data> where |encoded-data| is base64 URL
  // encoded string representation of the source URL.
  std::string source_string = source.GetString().Utf8();
  String encoded_source = WTF::Base64URLEncode(
      source_string.data(), SafeCast<unsigned>(source_string.length()));

  return KURL("remote-playback://" + encoded_source);
}

bool IsBackgroundAvailabilityMonitoringDisabled() {
  return MemoryPressureListenerRegistry::IsLowEndDevice();
}

}  // anonymous namespace

// static
RemotePlayback& RemotePlayback::From(HTMLMediaElement& element) {
  RemotePlayback* self =
      static_cast<RemotePlayback*>(RemotePlaybackController::From(element));
  if (!self) {
    self = MakeGarbageCollected<RemotePlayback>(element);
    RemotePlaybackController::ProvideTo(element, self);
  }
  return *self;
}

RemotePlayback::RemotePlayback(HTMLMediaElement& element)
    : ExecutionContextLifecycleObserver(element.GetExecutionContext()),
      RemotePlaybackController(element),
      state_(mojom::blink::PresentationConnectionState::CLOSED),
      availability_(mojom::ScreenAvailability::UNKNOWN),
      media_element_(&element),
      is_listening_(false),
      presentation_connection_receiver_(this, element.GetExecutionContext()),
      target_presentation_connection_(element.GetExecutionContext()) {}

const AtomicString& RemotePlayback::InterfaceName() const {
  return event_target_names::kRemotePlayback;
}

ExecutionContext* RemotePlayback::GetExecutionContext() const {
  return ExecutionContextLifecycleObserver::GetExecutionContext();
}

ScriptPromise RemotePlayback::watchAvailability(
    ScriptState* script_state,
    V8RemotePlaybackAvailabilityCallback* callback) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  if (media_element_->FastHasAttribute(
          html_names::kDisableremoteplaybackAttr)) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError,
        "disableRemotePlayback attribute is present."));
    return promise;
  }

  int id = WatchAvailabilityInternal(
      MakeGarbageCollected<AvailabilityCallbackWrapper>(callback));
  if (id == kWatchAvailabilityNotSupported) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotSupportedError,
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
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  if (media_element_->FastHasAttribute(
          html_names::kDisableremoteplaybackAttr)) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError,
        "disableRemotePlayback attribute is present."));
    return promise;
  }

  if (!CancelWatchAvailabilityInternal(id)) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotFoundError,
        "A callback with the given id is not found."));
    return promise;
  }

  resolver->Resolve();
  return promise;
}

ScriptPromise RemotePlayback::cancelWatchAvailability(
    ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  if (media_element_->FastHasAttribute(
          html_names::kDisableremoteplaybackAttr)) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError,
        "disableRemotePlayback attribute is present."));
    return promise;
  }

  availability_callbacks_.clear();
  StopListeningForAvailability();

  resolver->Resolve();
  return promise;
}

ScriptPromise RemotePlayback::prompt(ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  if (media_element_->FastHasAttribute(
          html_names::kDisableremoteplaybackAttr)) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError,
        "disableRemotePlayback attribute is present."));
    return promise;
  }

  if (prompt_promise_resolver_) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kOperationError,
        "A prompt is already being shown for this media element."));
    return promise;
  }

  if (!LocalFrame::HasTransientUserActivation(media_element_->GetFrame())) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidAccessError,
        "RemotePlayback::prompt() requires user gesture."));
    return promise;
  }

  if (!RuntimeEnabledFeatures::RemotePlaybackBackendEnabled()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotSupportedError,
        "The RemotePlayback API is disabled on this platform."));
    return promise;
  }

  if (availability_ == mojom::ScreenAvailability::UNAVAILABLE) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotFoundError, "No remote playback devices found."));
    return promise;
  }

  if (availability_ == mojom::ScreenAvailability::SOURCE_NOT_SUPPORTED) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotSupportedError,
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
  if (!GetExecutionContext())
    return;

  PresentationController* controller =
      PresentationController::FromContext(GetExecutionContext());
  if (controller && !availability_urls_.IsEmpty()) {
    controller->GetPresentationService()->StartPresentation(
        availability_urls_,
        WTF::Bind(&RemotePlayback::HandlePresentationResponse,
                  WrapPersistent(this)));
  } else {
    // TODO(yuryu): Wrapping PromptCancelled with base::OnceClosure as
    // InspectorInstrumentation requires a globally unique pointer to track
    // tasks. We can remove the wrapper if InspectorInstrumentation returns a
    // task id.
    base::OnceClosure task =
        WTF::Bind(&RemotePlayback::PromptCancelled, WrapPersistent(this));

    std::unique_ptr<probe::AsyncTaskId> task_id =
        std::make_unique<probe::AsyncTaskId>();
    probe::AsyncTaskScheduled(GetExecutionContext(), "promptCancelled",
                              task_id.get());
    GetExecutionContext()
        ->GetTaskRunner(TaskType::kMediaElementEvent)
        ->PostTask(FROM_HERE, WTF::Bind(RunRemotePlaybackTask,
                                        WrapPersistent(GetExecutionContext()),
                                        WTF::Passed(std::move(task)),
                                        WTF::Passed(std::move(task_id))));
  }
}

int RemotePlayback::WatchAvailabilityInternal(
    AvailabilityCallbackWrapper* callback) {
  if (RuntimeEnabledFeatures::RemotePlaybackBackendEnabled() &&
      IsBackgroundAvailabilityMonitoringDisabled()) {
    return kWatchAvailabilityNotSupported;
  }

  if (!GetExecutionContext())
    return kWatchAvailabilityNotSupported;

  int id;
  do {
    id = GetExecutionContext()->CircularSequentialID();
  } while (!availability_callbacks_.insert(id, callback).is_new_entry);

  // Report the current availability via the callback.
  // TODO(yuryu): Wrapping notifyInitialAvailability with base::OnceClosure as
  // InspectorInstrumentation requires a globally unique pointer to track tasks.
  // We can remove the wrapper if InspectorInstrumentation returns a task id.
  base::OnceClosure task = WTF::Bind(&RemotePlayback::NotifyInitialAvailability,
                                     WrapPersistent(this), id);
  std::unique_ptr<probe::AsyncTaskId> task_id =
      std::make_unique<probe::AsyncTaskId>();
  probe::AsyncTaskScheduled(GetExecutionContext(), "watchAvailabilityCallback",
                            task_id.get());
  GetExecutionContext()
      ->GetTaskRunner(TaskType::kMediaElementEvent)
      ->PostTask(FROM_HERE, WTF::Bind(RunRemotePlaybackTask,
                                      WrapPersistent(GetExecutionContext()),
                                      WTF::Passed(std::move(task)),
                                      WTF::Passed(std::move(task_id))));

  MaybeStartListeningForAvailability();
  return id;
}

bool RemotePlayback::CancelWatchAvailabilityInternal(int id) {
  if (id <= 0)  // HashMap doesn't support the cases of key = 0 or key = -1.
    return false;
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

void RemotePlayback::StateChanged(
    mojom::blink::PresentationConnectionState state) {
  if (prompt_promise_resolver_) {
    // Changing state to "disconnected" from "disconnected" or "connecting"
    // means that establishing connection with remote playback device failed.
    // Changing state to anything else means the state change intended by
    // prompt() succeeded.
    if (state_ != mojom::blink::PresentationConnectionState::CONNECTED &&
        state == mojom::blink::PresentationConnectionState::CLOSED) {
      prompt_promise_resolver_->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kAbortError,
          "Failed to connect to the remote device."));
    } else {
      DCHECK((state_ == mojom::blink::PresentationConnectionState::CLOSED &&
              state == mojom::blink::PresentationConnectionState::CONNECTING) ||
             (state_ == mojom::blink::PresentationConnectionState::CONNECTED &&
              state == mojom::blink::PresentationConnectionState::CLOSED));
      prompt_promise_resolver_->Resolve();
    }
    prompt_promise_resolver_ = nullptr;
  }

  if (state_ == state)
    return;

  state_ = state;
  if (state_ == mojom::blink::PresentationConnectionState::CONNECTING) {
    DispatchEvent(*Event::Create(event_type_names::kConnecting));
    if (auto* video_element =
            DynamicTo<HTMLVideoElement>(media_element_.Get())) {
      // TODO(xjz): Pass the remote device name.

      video_element->MediaRemotingStarted(WebString());
    }
    media_element_->FlingingStarted();
  } else if (state_ == mojom::blink::PresentationConnectionState::CONNECTED) {
    DispatchEvent(*Event::Create(event_type_names::kConnect));
  } else if (state_ == mojom::blink::PresentationConnectionState::CLOSED ||
             state_ == mojom::blink::PresentationConnectionState::TERMINATED) {
    DispatchEvent(*Event::Create(event_type_names::kDisconnect));
    if (auto* video_element =
            DynamicTo<HTMLVideoElement>(media_element_.Get())) {
      video_element->MediaRemotingStopped(
          WebMediaPlayerClient::kMediaRemotingStopNoText);
    }
    CleanupConnections();
    presentation_id_ = "";
    presentation_url_ = KURL();
    media_element_->FlingingStopped();
  }

  for (auto observer : observers_)
    observer->OnRemotePlaybackStateChanged(state_);
}

void RemotePlayback::PromptCancelled() {
  if (!prompt_promise_resolver_)
    return;

  prompt_promise_resolver_->Reject(MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kNotAllowedError, "The prompt was dismissed."));
  prompt_promise_resolver_ = nullptr;
}

void RemotePlayback::SourceChanged(const WebURL& source,
                                   bool is_source_supported) {
  if (IsBackgroundAvailabilityMonitoringDisabled())
    return;

  KURL current_url =
      availability_urls_.IsEmpty() ? KURL() : availability_urls_[0];
  KURL new_url = GetAvailabilityUrl(source, is_source_supported);

  if (new_url == current_url)
    return;

  // Tell PresentationController to stop listening for availability before the
  // URLs vector is updated.
  StopListeningForAvailability();

  availability_urls_.clear();
  if (!new_url.IsEmpty())
    availability_urls_.push_back(new_url);

  MaybeStartListeningForAvailability();
}

WebString RemotePlayback::GetPresentationId() {
  return presentation_id_;
}

void RemotePlayback::AddObserver(RemotePlaybackObserver* observer) {
  observers_.insert(observer);
}

void RemotePlayback::RemoveObserver(RemotePlaybackObserver* observer) {
  observers_.erase(observer);
}

void RemotePlayback::AvailabilityChangedForTesting(bool screen_is_available) {
  // AvailabilityChanged() is only normally called when |is_listening_| is true.
  is_listening_ = true;
  AvailabilityChanged(screen_is_available
                          ? mojom::blink::ScreenAvailability::AVAILABLE
                          : mojom::blink::ScreenAvailability::UNAVAILABLE);
}

void RemotePlayback::StateChangedForTesting(bool is_connected) {
  StateChanged(is_connected
                   ? mojom::blink::PresentationConnectionState::CONNECTED
                   : mojom::blink::PresentationConnectionState::CLOSED);
}

bool RemotePlayback::RemotePlaybackAvailable() const {
  if (IsBackgroundAvailabilityMonitoringDisabled() &&
      RuntimeEnabledFeatures::RemotePlaybackBackendEnabled() &&
      !media_element_->currentSrc().IsEmpty()) {
    return true;
  }

  return availability_ == mojom::ScreenAvailability::AVAILABLE;
}

void RemotePlayback::RemotePlaybackDisabled() {
  if (prompt_promise_resolver_) {
    prompt_promise_resolver_->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError,
        "disableRemotePlayback attribute is present."));
    prompt_promise_resolver_ = nullptr;
  }

  availability_callbacks_.clear();
  StopListeningForAvailability();

  if (state_ == mojom::blink::PresentationConnectionState::CLOSED ||
      state_ == mojom::blink::PresentationConnectionState::TERMINATED) {
    return;
  }

  auto* controller = PresentationController::FromContext(GetExecutionContext());
  if (controller) {
    controller->GetPresentationService()->CloseConnection(presentation_url_,
                                                          presentation_id_);
  }
}

void RemotePlayback::CleanupConnections() {
  target_presentation_connection_.reset();
  presentation_connection_receiver_.reset();
}

void RemotePlayback::AvailabilityChanged(
    mojom::blink::ScreenAvailability availability) {
  DCHECK(is_listening_);
  DCHECK_NE(availability, mojom::ScreenAvailability::UNKNOWN);
  DCHECK_NE(availability, mojom::ScreenAvailability::DISABLED);

  if (availability_ == availability)
    return;

  bool old_availability = RemotePlaybackAvailable();
  availability_ = availability;
  bool new_availability = RemotePlaybackAvailable();
  if (new_availability == old_availability)
    return;

  // Copy the callbacks to a temporary vector to prevent iterator invalidations,
  // in case the JS callbacks invoke watchAvailability().
  HeapVector<Member<AvailabilityCallbackWrapper>> callbacks;
  CopyValuesToVector(availability_callbacks_, callbacks);

  for (auto& callback : callbacks)
    callback->Run(this, new_availability);
}

const Vector<KURL>& RemotePlayback::Urls() const {
  // TODO(avayvod): update the URL format and add frame url, mime type and
  // response headers when available.
  return availability_urls_;
}

void RemotePlayback::OnConnectionSuccess(
    mojom::blink::PresentationConnectionResultPtr result) {
  presentation_id_ = std::move(result->presentation_info->id);
  presentation_url_ = std::move(result->presentation_info->url);

  StateChanged(mojom::blink::PresentationConnectionState::CONNECTING);

  DCHECK(!presentation_connection_receiver_.is_bound());
  auto* presentation_controller =
      PresentationController::FromContext(GetExecutionContext());
  if (!presentation_controller)
    return;

  // Note: Messages on |connection_receiver| are ignored.
  target_presentation_connection_.Bind(
      std::move(result->connection_remote),
      GetExecutionContext()->GetTaskRunner(TaskType::kMediaElementEvent));
  presentation_connection_receiver_.Bind(
      std::move(result->connection_receiver),
      GetExecutionContext()->GetTaskRunner(TaskType::kMediaElementEvent));
}

void RemotePlayback::OnConnectionError(
    const mojom::blink::PresentationError& error) {
  presentation_id_ = "";
  presentation_url_ = KURL();
  if (error.error_type ==
      mojom::blink::PresentationErrorType::PRESENTATION_REQUEST_CANCELLED) {
    PromptCancelled();
    return;
  }

  StateChanged(mojom::blink::PresentationConnectionState::CLOSED);
}

void RemotePlayback::HandlePresentationResponse(
    mojom::blink::PresentationConnectionResultPtr result,
    mojom::blink::PresentationErrorPtr error) {
  if (result) {
    OnConnectionSuccess(std::move(result));
  } else {
    OnConnectionError(*error);
  }
}

void RemotePlayback::OnMessage(
    mojom::blink::PresentationConnectionMessagePtr message) {
  // Messages are ignored.
}

void RemotePlayback::DidChangeState(
    mojom::blink::PresentationConnectionState state) {
  StateChanged(state);
}

void RemotePlayback::DidClose(
    mojom::blink::PresentationConnectionCloseReason reason) {
  StateChanged(mojom::blink::PresentationConnectionState::CLOSED);
}

void RemotePlayback::StopListeningForAvailability() {
  if (!is_listening_)
    return;

  availability_ = mojom::ScreenAvailability::UNKNOWN;
  PresentationController* controller =
      PresentationController::FromContext(GetExecutionContext());
  if (!controller)
    return;

  controller->RemoveAvailabilityObserver(this);
  is_listening_ = false;
}

void RemotePlayback::MaybeStartListeningForAvailability() {
  if (IsBackgroundAvailabilityMonitoringDisabled())
    return;

  if (is_listening_)
    return;

  if (availability_urls_.IsEmpty() || availability_callbacks_.IsEmpty())
    return;

  PresentationController* controller =
      PresentationController::FromContext(GetExecutionContext());
  if (!controller)
    return;

  controller->AddAvailabilityObserver(this);
  is_listening_ = true;
}

void RemotePlayback::Trace(Visitor* visitor) {
  visitor->Trace(availability_callbacks_);
  visitor->Trace(prompt_promise_resolver_);
  visitor->Trace(media_element_);
  visitor->Trace(presentation_connection_receiver_);
  visitor->Trace(target_presentation_connection_);
  visitor->Trace(observers_);
  EventTargetWithInlineData::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
  RemotePlaybackController::Trace(visitor);
}

}  // namespace blink
