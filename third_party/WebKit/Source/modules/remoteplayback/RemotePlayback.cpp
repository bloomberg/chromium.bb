// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/remoteplayback/RemotePlayback.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/Document.h"
#include "core/events/Event.h"
#include "core/html/HTMLMediaElement.h"
#include "modules/EventTargetModules.h"
#include "modules/remoteplayback/RemotePlaybackAvailability.h"

namespace blink {

namespace {

const AtomicString& remotePlaybackStateToString(WebRemotePlaybackState state)
{
    DEFINE_STATIC_LOCAL(const AtomicString, connectedValue, ("connected", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(const AtomicString, disconnectedValue, ("disconnected", AtomicString::ConstructFromLiteral));

    switch (state) {
    case WebRemotePlaybackState::Connected:
        return connectedValue;
    case WebRemotePlaybackState::Disconnected:
        return disconnectedValue;
    }

    ASSERT_NOT_REACHED();
    return disconnectedValue;
}

} // anonymous namespace

// static
RemotePlayback* RemotePlayback::create(HTMLMediaElement& element)
{
    ASSERT(element.document().frame());

    RemotePlayback* remotePlayback = new RemotePlayback(
        element.document().frame(),
        element.isPlayingRemotely() ? WebRemotePlaybackState::Connected : WebRemotePlaybackState::Disconnected,
        element.hasRemoteRoutes());
    element.setRemotePlaybackClient(remotePlayback);

    return remotePlayback;
}

RemotePlayback::RemotePlayback(LocalFrame* frame, WebRemotePlaybackState state, bool availability)
    : DOMWindowProperty(frame)
    , m_state(state)
    , m_availability(availability)
{
}

const AtomicString& RemotePlayback::interfaceName() const
{
    return EventTargetNames::RemotePlayback;
}

ExecutionContext* RemotePlayback::getExecutionContext() const
{
    if (!m_frame)
        return 0;
    return m_frame->document();
}

ScriptPromise RemotePlayback::getAvailability(ScriptState* scriptState)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    // TODO(avayvod): currently the availability is tracked for each media element
    // as soon as it's created, we probably want to limit that to when the page/element
    // is visible (see https://crbug.com/597281) and has default controls. If there's
    // no default controls, we should also start tracking availability on demand
    // meaning the Promise returned by getAvailability() will be resolved asynchronously.
    RemotePlaybackAvailability* availability = RemotePlaybackAvailability::take(resolver, m_availability);
    m_availabilityObjects.append(availability);
    resolver->resolve(availability);
    return promise;
}

String RemotePlayback::state() const
{
    return remotePlaybackStateToString(m_state);
}

void RemotePlayback::stateChanged(WebRemotePlaybackState state)
{
    if (m_state == state)
        return;

    m_state = state;
    dispatchEvent(Event::create(EventTypeNames::statechange));
}

void RemotePlayback::availabilityChanged(bool available)
{
    if (m_availability == available)
        return;

    m_availability = available;
    for (auto& availabilityObject : m_availabilityObjects)
        availabilityObject->availabilityChanged(available);
}

DEFINE_TRACE(RemotePlayback)
{
    visitor->trace(m_availabilityObjects);
    RefCountedGarbageCollectedEventTargetWithInlineData<RemotePlayback>::trace(visitor);
    DOMWindowProperty::trace(visitor);
}

} // namespace blink
