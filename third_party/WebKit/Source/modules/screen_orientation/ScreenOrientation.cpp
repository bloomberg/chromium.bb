// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/screen_orientation/ScreenOrientation.h"

#include "core/frame/DOMWindow.h"
#include "core/frame/Frame.h"
#include "core/frame/Screen.h"
#include "modules/screen_orientation/ScreenOrientationController.h"
#include "public/platform/WebScreenOrientation.h"

namespace WebCore {

struct ScreenOrientationInfo {
    const AtomicString& name;
    blink::WebScreenOrientation orientation;
};

static ScreenOrientationInfo* orientationsMap(unsigned& length)
{
    DEFINE_STATIC_LOCAL(const AtomicString, portraitPrimary, ("portrait-primary", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(const AtomicString, portraitSecondary, ("portrait-secondary", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(const AtomicString, landscapePrimary, ("landscape-primary", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(const AtomicString, landscapeSecondary, ("landscape-secondary", AtomicString::ConstructFromLiteral));

    static ScreenOrientationInfo orientationMap[] = {
        { portraitPrimary, blink::WebScreenOrientationPortraitPrimary },
        { portraitSecondary, blink::WebScreenOrientationPortraitSecondary },
        { landscapePrimary, blink::WebScreenOrientationLandscapePrimary },
        { landscapeSecondary, blink::WebScreenOrientationLandscapeSecondary }
    };
    length = WTF_ARRAY_LENGTH(orientationMap);
    return orientationMap;
}

static const AtomicString& orientationToString(blink::WebScreenOrientation orientation)
{
    unsigned length = 0;
    ScreenOrientationInfo* orientationMap = orientationsMap(length);
    for (unsigned i = 0; i < length; ++i) {
        if (orientationMap[i].orientation == orientation)
            return orientationMap[i].name;
    }
    // We do no handle OrientationInvalid and OrientationAny but this is fine because screen.orientation
    // should never return these and WebScreenOrientation does not define those values.
    ASSERT_NOT_REACHED();
    return nullAtom;
}

ScreenOrientation::ScreenOrientation(Screen& screen)
    : DOMWindowProperty(screen.frame())
{
}

const char* ScreenOrientation::supplementName()
{
    return "ScreenOrientation";
}

Document& ScreenOrientation::document() const
{
    ASSERT(m_associatedDOMWindow);
    ASSERT(m_associatedDOMWindow->document());
    return *m_associatedDOMWindow->document();
}

ScreenOrientation& ScreenOrientation::from(Screen& screen)
{
    ScreenOrientation* supplement = static_cast<ScreenOrientation*>(Supplement<Screen>::from(screen, supplementName()));
    if (!supplement) {
        supplement = new ScreenOrientation(screen);
        provideTo(screen, supplementName(), adoptPtr(supplement));
    }
    return *supplement;
}

ScreenOrientation::~ScreenOrientation()
{
}

const AtomicString& ScreenOrientation::orientation(Screen& screen)
{
    ScreenOrientation& screenOrientation = ScreenOrientation::from(screen);
    ScreenOrientationController& controller = ScreenOrientationController::from(screenOrientation.document());
    return orientationToString(controller.orientation());
}

bool ScreenOrientation::lockOrientation(Screen& screen, const Vector<String>& orientations)
{
    // FIXME: Implement.
    return false;
}

bool ScreenOrientation::lockOrientation(Screen& screen, const AtomicString& orientation)
{
    // FIXME: Implement.
    return false;
}

void ScreenOrientation::unlockOrientation(Screen& screen)
{
    // FIXME: Implement.
}

} // namespace WebCore
