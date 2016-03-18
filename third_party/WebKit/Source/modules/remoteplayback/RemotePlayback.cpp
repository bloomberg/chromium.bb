// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/remoteplayback/RemotePlayback.h"

#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "modules/EventTargetModules.h"

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
RemotePlayback* RemotePlayback::create(LocalFrame* frame)
{
    ASSERT(frame);

    RemotePlayback* remotePlayback = new RemotePlayback(frame);

    return remotePlayback;
}

RemotePlayback::RemotePlayback(LocalFrame* frame)
    : DOMWindowProperty(frame)
    , m_state(WebRemotePlaybackState::Disconnected)
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

String RemotePlayback::state() const
{
    return remotePlaybackStateToString(m_state);
}

DEFINE_TRACE(RemotePlayback)
{
    RefCountedGarbageCollectedEventTargetWithInlineData<RemotePlayback>::trace(visitor);
    DOMWindowProperty::trace(visitor);
}

} // namespace blink
