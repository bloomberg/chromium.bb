// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "modules/mediarecorder/MediaRecorderErrorEvent.h"

namespace blink {

// static
PassRefPtrWillBeRawPtr<MediaRecorderErrorEvent> MediaRecorderErrorEvent::create()
{
    return adoptRefWillBeNoop(new MediaRecorderErrorEvent);
}

// static
PassRefPtrWillBeRawPtr<MediaRecorderErrorEvent> MediaRecorderErrorEvent::create(const AtomicString& type, bool canBubble, bool cancelable, const String& name, const String& message)
{
    return adoptRefWillBeNoop(new MediaRecorderErrorEvent(type, canBubble, cancelable, name, message));
}

const AtomicString& MediaRecorderErrorEvent::interfaceName() const
{
    return EventNames::MediaRecorderErrorEvent;
}

MediaRecorderErrorEvent::MediaRecorderErrorEvent(const AtomicString& type, bool canBubble, bool cancelable, const String& name, const String& message)
    : Event(type, canBubble, cancelable)
    , m_name(name)
    , m_message(message)
{
}

} // namespace blink
