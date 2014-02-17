// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/screen_orientation/ScreenOrientation.h"

#include "core/frame/DOMWindow.h"
#include "core/frame/Frame.h"
#include "core/frame/Screen.h"

namespace WebCore {

ScreenOrientation::ScreenOrientation(Screen* screen)
    : DOMWindowProperty(screen->frame())
{
}

const char* ScreenOrientation::supplementName()
{
    return "ScreenOrientation";
}

ScreenOrientation* ScreenOrientation::from(Screen* screen)
{
    ScreenOrientation* supplement = static_cast<ScreenOrientation*>(Supplement<Screen>::from(screen, supplementName()));
    if (!supplement) {
        ASSERT(screen);
        supplement = new ScreenOrientation(screen);
        provideTo(screen, supplementName(), adoptPtr(supplement));
    }
    return supplement;
}

ScreenOrientation::~ScreenOrientation()
{
}

Screen* ScreenOrientation::screen() const
{
    Frame* frame = this->frame();
    ASSERT(frame);
    DOMWindow* window = frame->domWindow();
    ASSERT(window);
    return window->screen();
}

const AtomicString& ScreenOrientation::orientation(Screen* screen)
{
    // FIXME: Implement.
    DEFINE_STATIC_LOCAL(const AtomicString, portraitPrimary, ("portrait-primary", AtomicString::ConstructFromLiteral));
    return portraitPrimary;
}

bool ScreenOrientation::lockOrientation(Screen* screen, const Vector<String>& orientations)
{
    // FIXME: Implement.
    return false;
}

bool ScreenOrientation::lockOrientation(Screen* screen, const AtomicString& orientation)
{
    // FIXME: Implement.
    return false;
}

void ScreenOrientation::unlockOrientation(Screen* screen)
{
    // FIXME: Implement.
}

} // namespace WebCore
