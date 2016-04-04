// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InputEvent_h
#define InputEvent_h

#include "core/events/InputEventInit.h"
#include "core/events/UIEvent.h"

namespace blink {

class InputEvent final : public UIEvent {
    DEFINE_WRAPPERTYPEINFO();

public:
    static InputEvent* create()
    {
        return new InputEvent;
    }

    static InputEvent* create(const AtomicString& type, const InputEventInit& initializer)
    {
        return new InputEvent(type, initializer);
    }

    bool isInputEvent() const override;

    DECLARE_VIRTUAL_TRACE();

private:
    InputEvent();
    InputEvent(const AtomicString&, const InputEventInit&);
};

DEFINE_EVENT_TYPE_CASTS(InputEvent);

} // namespace blink

#endif // InputEvent_h
