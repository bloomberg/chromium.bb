// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/remoteplayback/RemotePlayback.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/modules/v8/RemotePlaybackAvailabilityCallback.h"
#include "core/HTMLNames.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContextTask.h"
#include "core/events/Event.h"
#include "core/html/HTMLMediaElement.h"
#include "modules/EventTargetModules.h"
#include "platform/UserGestureIndicator.h"

namespace blink {

namespace {

const AtomicString& remotePlaybackStateToString(WebRemotePlaybackState state) {
  DEFINE_STATIC_LOCAL(const AtomicString, connectingValue, ("connecting"));
  DEFINE_STATIC_LOCAL(const AtomicString, connectedValue, ("connected"));
  DEFINE_STATIC_LOCAL(const AtomicString, disconnectedValue, ("disconnected"));

  switch (state) {
    case WebRemotePlaybackState::Connecting:
      return connectingValue;
    case WebRemotePlaybackState::Connected:
      return connectedValue;
    case WebRemotePlaybackState::Disconnected:
      return disconnectedValue;
  }

  ASSERT_NOT_REACHED();
  return disconnectedValue;
}

}  // anonymous namespace

// static
RemotePlayback* RemotePlayback::create(ScriptState* scriptState,
                                       HTMLMediaElement& element) {
  DCHECK(element.document().frame());
  DCHECK(scriptState);

  RemotePlayback* remotePlayback = new RemotePlayback(scriptState, element);
  element.setRemotePlaybackClient(remotePlayback);

  return remotePlayback;
}

RemotePlayback::RemotePlayback(ScriptState* scriptState,
                               HTMLMediaElement& element)
    : ActiveScriptWrappable(this),
      m_scriptState(scriptState),
      m_state(element.isPlayingRemotely()
                  ? WebRemotePlaybackState::Connected
                  : WebRemotePlaybackState::Disconnected),
      m_availability(element.hasRemoteRoutes()),
      m_mediaElement(&element) {}

const AtomicString& RemotePlayback::interfaceName() const {
  return EventTargetNames::RemotePlayback;
}

ExecutionContext* RemotePlayback::getExecutionContext() const {
  return &m_mediaElement->document();
}

ScriptPromise RemotePlayback::watchAvailability(
    RemotePlaybackAvailabilityCallback* callback) {
  ScriptPromiseResolver* resolver =
      ScriptPromiseResolver::create(m_scriptState.get());
  ScriptPromise promise = resolver->promise();

  if (m_mediaElement->fastHasAttribute(HTMLNames::disableremoteplaybackAttr)) {
    resolver->reject(DOMException::create(
        InvalidStateError, "disableRemotePlayback attribute is present."));
    return promise;
  }

  // TODO(avayvod): implement steps 4 and 5 of the algorithm.
  // https://crbug.com/655233
  int id;
  do {
    id = getExecutionContext()->circularSequentialID();
  } while (!m_availabilityCallbacks.add(id, callback).isNewEntry);

  // Report the current availability via the callback.
  getExecutionContext()->postTask(
      BLINK_FROM_HERE,
      createSameThreadTask(&RemotePlayback::notifyInitialAvailability,
                           wrapPersistent(this), id),
      "watchAvailabilityCallback");

  // TODO(avayvod): Currently the availability is tracked for each media element
  // as soon as it's created, we probably want to limit that to when the
  // page/element is visible (see https://crbug.com/597281) and has default
  // controls. If there are no default controls, we should also start tracking
  // availability on demand meaning the Promise returned by watchAvailability()
  // will be resolved asynchronously.
  resolver->resolve(id);
  return promise;
}

ScriptPromise RemotePlayback::cancelWatchAvailability(int id) {
  ScriptPromiseResolver* resolver =
      ScriptPromiseResolver::create(m_scriptState.get());
  ScriptPromise promise = resolver->promise();

  if (m_mediaElement->fastHasAttribute(HTMLNames::disableremoteplaybackAttr)) {
    resolver->reject(DOMException::create(
        InvalidStateError, "disableRemotePlayback attribute is present."));
    return promise;
  }

  auto iter = m_availabilityCallbacks.find(id);
  if (iter == m_availabilityCallbacks.end()) {
    resolver->reject(DOMException::create(
        NotFoundError, "A callback with the given id is not found."));
    return promise;
  }

  m_availabilityCallbacks.remove(iter);

  resolver->resolve();
  return promise;
}

ScriptPromise RemotePlayback::cancelWatchAvailability() {
  ScriptPromiseResolver* resolver =
      ScriptPromiseResolver::create(m_scriptState.get());
  ScriptPromise promise = resolver->promise();

  if (m_mediaElement->fastHasAttribute(HTMLNames::disableremoteplaybackAttr)) {
    resolver->reject(DOMException::create(
        InvalidStateError, "disableRemotePlayback attribute is present."));
    return promise;
  }

  m_availabilityCallbacks.clear();

  resolver->resolve();
  return promise;
}

ScriptPromise RemotePlayback::prompt() {
  // TODO(avayvod): implement steps 5, 8, 9 of the algorithm.
  // https://crbug.com/647441
  ScriptPromiseResolver* resolver =
      ScriptPromiseResolver::create(m_scriptState.get());
  ScriptPromise promise = resolver->promise();

  if (m_mediaElement->fastHasAttribute(HTMLNames::disableremoteplaybackAttr)) {
    resolver->reject(DOMException::create(
        InvalidStateError, "disableRemotePlayback attribute is present."));
    return promise;
  }

  if (m_promptPromiseResolver) {
    resolver->reject(DOMException::create(
        OperationError,
        "A prompt is already being shown for this media element."));
    return promise;
  }

  if (!UserGestureIndicator::utilizeUserGesture()) {
    resolver->reject(DOMException::create(
        InvalidAccessError, "RemotePlayback::prompt() requires user gesture."));
    return promise;
  }

  if (m_state == WebRemotePlaybackState::Disconnected) {
    m_promptPromiseResolver = resolver;
    m_mediaElement->requestRemotePlayback();
  } else {
    m_mediaElement->requestRemotePlaybackControl();
    // TODO(avayvod): Need to keep the resolver until user chooses to stop
    // the remote playback (resolve) or dismisses the UI (reject).
    // Steps 11 and 12 of the prompt() algorithm.
    // https://crbug.com/647441
    resolver->resolve();
  }

  return promise;
}

String RemotePlayback::state() const {
  return remotePlaybackStateToString(m_state);
}

bool RemotePlayback::hasPendingActivity() const {
  return hasEventListeners() || !m_availabilityCallbacks.isEmpty() ||
         m_promptPromiseResolver;
}

void RemotePlayback::notifyInitialAvailability(int callbackId) {
  // May not find the callback if the website cancels it fast enough.
  auto iter = m_availabilityCallbacks.find(callbackId);
  if (iter == m_availabilityCallbacks.end())
    return;

  iter->value->call(this, m_availability);
}

void RemotePlayback::stateChanged(WebRemotePlaybackState state) {
  // We may get a "disconnected" state change while in the "disconnected"
  // state if initiated connection fails. So cleanup the promise resolvers
  // before checking if anything changed.
  // TODO(avayvod): cleanup this logic when we implementing the "connecting"
  // state.
  if (m_promptPromiseResolver) {
    if (state != WebRemotePlaybackState::Disconnected)
      m_promptPromiseResolver->resolve();
    else
      m_promptPromiseResolver->reject(DOMException::create(
          AbortError, "Failed to connect to the remote device."));
    m_promptPromiseResolver = nullptr;
  }

  if (m_state == state)
    return;

  m_state = state;
  switch (m_state) {
    case WebRemotePlaybackState::Connecting:
      dispatchEvent(Event::create(EventTypeNames::connecting));
      break;
    case WebRemotePlaybackState::Connected:
      dispatchEvent(Event::create(EventTypeNames::connect));
      break;
    case WebRemotePlaybackState::Disconnected:
      dispatchEvent(Event::create(EventTypeNames::disconnect));
      break;
  }
}

void RemotePlayback::availabilityChanged(bool available) {
  if (m_availability == available)
    return;

  m_availability = available;
  for (auto& callback : m_availabilityCallbacks.values())
    callback->call(this, m_availability);
}

void RemotePlayback::promptCancelled() {
  if (!m_promptPromiseResolver)
    return;

  m_promptPromiseResolver->reject(
      DOMException::create(NotAllowedError, "The prompt was dismissed."));
  m_promptPromiseResolver = nullptr;
}

void RemotePlayback::remotePlaybackDisabled() {
  if (m_promptPromiseResolver) {
    m_promptPromiseResolver->reject(DOMException::create(
        InvalidStateError, "disableRemotePlayback attribute is present."));
    m_promptPromiseResolver = nullptr;
  }

  m_availabilityCallbacks.clear();

  // TODO(avayvod): stop remote playback too. https://crbug.com/657566.
}

void RemotePlayback::setV8ReferencesForCallbacks(
    v8::Isolate* isolate,
    const v8::Persistent<v8::Object>& wrapper) {
  for (auto callback : m_availabilityCallbacks.values())
    callback->setWrapperReference(isolate, wrapper);
}

DEFINE_TRACE(RemotePlayback) {
  visitor->trace(m_availabilityCallbacks);
  visitor->trace(m_promptPromiseResolver);
  visitor->trace(m_mediaElement);
  EventTargetWithInlineData::trace(visitor);
}

DEFINE_TRACE_WRAPPERS(RemotePlayback) {
  for (auto callback : m_availabilityCallbacks.values()) {
    visitor->traceWrappers(callback);
  }
  EventTargetWithInlineData::traceWrappers(visitor);
}

}  // namespace blink
