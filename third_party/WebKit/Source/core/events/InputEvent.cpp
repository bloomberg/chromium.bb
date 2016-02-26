// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/events/InputEvent.h"

#include "core/events/EventDispatcher.h"

namespace blink {

InputEvent::InputEvent()
{
}

InputEvent::InputEvent(const AtomicString& type, const InputEventInit& initializer)
    : UIEvent(type, initializer)
{
}

bool InputEvent::isInputEvent() const
{
    return true;
}

DEFINE_TRACE(InputEvent)
{
    UIEvent::trace(visitor);
}

} // namespace blink
