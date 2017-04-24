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
#include "core/events/Event.h"
#include "core/html/HTMLMediaElement.h"
#include "core/probe/CoreProbes.h"
#include "modules/EventTargetModules.h"
#include "platform/MemoryCoordinator.h"
#include "platform/UserGestureIndicator.h"

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
      media_element_(&element) {}

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

  if (MemoryCoordinator::IsLowEndDevice()) {
    resolver->Reject(DOMException::Create(
        kNotSupportedError,
        "Availability monitoring is not supported on this device."));
    return promise;
  }

  int id;
  do {
    id = GetExecutionContext()->CircularSequentialID();
  } while (
      !availability_callbacks_
           .insert(id, TraceWrapperMember<RemotePlaybackAvailabilityCallback>(
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

  auto iter = availability_callbacks_.Find(id);
  if (iter == availability_callbacks_.end()) {
    resolver->Reject(DOMException::Create(
        kNotFoundError, "A callback with the given id is not found."));
    return promise;
  }

  availability_callbacks_.erase(iter);

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

  if (!UserGestureIndicator::UtilizeUserGesture()) {
    resolver->Reject(DOMException::Create(
        kInvalidAccessError,
        "RemotePlayback::prompt() requires user gesture."));
    return promise;
  }

  // TODO(avayvod): don't do this check on low-end devices - merge with
  // https://codereview.chromium.org/2475293003
  if (availability_ == WebRemotePlaybackAvailability::kDeviceNotAvailable) {
    resolver->Reject(DOMException::Create(kNotFoundError,
                                          "No remote playback devices found."));
    return promise;
  }

  if (availability_ == WebRemotePlaybackAvailability::kSourceNotSupported ||
      availability_ == WebRemotePlaybackAvailability::kSourceNotCompatible) {
    resolver->Reject(DOMException::Create(
        kNotSupportedError,
        "The currentSrc is not compatible with remote playback"));
    return promise;
  }

  if (state_ == WebRemotePlaybackState::kDisconnected) {
    prompt_promise_resolver_ = resolver;
    media_element_->RequestRemotePlayback();
  } else {
    prompt_promise_resolver_ = resolver;
    media_element_->RequestRemotePlaybackControl();
  }

  return promise;
}

String RemotePlayback::state() const {
  return RemotePlaybackStateToString(state_);
}

bool RemotePlayback::HasPendingActivity() const {
  return HasEventListeners() || !availability_callbacks_.IsEmpty() ||
         prompt_promise_resolver_;
}

void RemotePlayback::NotifyInitialAvailability(int callback_id) {
  // May not find the callback if the website cancels it fast enough.
  auto iter = availability_callbacks_.Find(callback_id);
  if (iter == availability_callbacks_.end())
    return;

  iter->value->call(this, RemotePlaybackAvailable());
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
      break;
    case WebRemotePlaybackState::kConnected:
      DispatchEvent(Event::Create(EventTypeNames::connect));
      break;
    case WebRemotePlaybackState::kDisconnected:
      DispatchEvent(Event::Create(EventTypeNames::disconnect));
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
    callback->call(this, new_availability);
}

void RemotePlayback::PromptCancelled() {
  if (!prompt_promise_resolver_)
    return;

  prompt_promise_resolver_->Reject(
      DOMException::Create(kNotAllowedError, "The prompt was dismissed."));
  prompt_promise_resolver_ = nullptr;
}

bool RemotePlayback::RemotePlaybackAvailable() const {
  return availability_ == WebRemotePlaybackAvailability::kDeviceAvailable;
}

void RemotePlayback::RemotePlaybackDisabled() {
  if (prompt_promise_resolver_) {
    prompt_promise_resolver_->Reject(DOMException::Create(
        kInvalidStateError, "disableRemotePlayback attribute is present."));
    prompt_promise_resolver_ = nullptr;
  }

  availability_callbacks_.clear();

  if (state_ != WebRemotePlaybackState::kDisconnected)
    media_element_->RequestRemotePlaybackStop();
}

DEFINE_TRACE(RemotePlayback) {
  visitor->Trace(availability_callbacks_);
  visitor->Trace(prompt_promise_resolver_);
  visitor->Trace(media_element_);
  EventTargetWithInlineData::Trace(visitor);
}

DEFINE_TRACE_WRAPPERS(RemotePlayback) {
  for (auto callback : availability_callbacks_.Values()) {
    visitor->TraceWrappers(callback);
  }
  EventTargetWithInlineData::TraceWrappers(visitor);
}

}  // namespace blink
