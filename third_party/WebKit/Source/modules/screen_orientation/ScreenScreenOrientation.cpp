// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/screen_orientation/ScreenScreenOrientation.h"

#include "bindings/core/v8/ScriptState.h"
#include "core/frame/Screen.h"
#include "modules/screen_orientation/ScreenOrientation.h"

namespace blink {

// static
ScreenScreenOrientation& ScreenScreenOrientation::from(Screen& screen)
{
    ScreenScreenOrientation* supplement = static_cast<ScreenScreenOrientation*>(WillBeHeapSupplement<Screen>::from(screen, supplementName()));
    if (!supplement) {
        supplement = new ScreenScreenOrientation();
        provideTo(screen, supplementName(), adoptPtrWillBeNoop(supplement));
    }
    return *supplement;
}

ScreenScreenOrientation::~ScreenScreenOrientation()
{
}

// static
ScreenOrientation* ScreenScreenOrientation::orientation(ScriptState* state, Screen& screen)
{
    ScreenScreenOrientation& self = ScreenScreenOrientation::from(screen);
    if (!screen.frame())
        return 0;

    if (!self.m_orientation)
        self.m_orientation = ScreenOrientation::create(screen.frame());

    return self.m_orientation.get();
}

const char* ScreenScreenOrientation::supplementName()
{
    return "ScreenScreenOrientation";
}

void ScreenScreenOrientation::trace(Visitor* visitor)
{
    visitor->trace(m_orientation);
    WillBeHeapSupplement<Screen>::trace(visitor);
}

} // namespace blink
