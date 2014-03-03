// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/screen_orientation/ScreenOrientation.h"

#include "core/frame/DOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Screen.h"
#include "modules/screen_orientation/ScreenOrientationController.h"
#include "public/platform/Platform.h"

namespace WebCore {

static const unsigned WebScreenOrientationDefault = 0;

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

static blink::WebScreenOrientations stringToOrientations(const AtomicString& orientationString)
{
    DEFINE_STATIC_LOCAL(const AtomicString, portrait, ("portrait", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(const AtomicString, landscape, ("landscape", AtomicString::ConstructFromLiteral));

    if (orientationString == portrait)
        return blink::WebScreenOrientationPortraitPrimary | blink::WebScreenOrientationPortraitSecondary;
    if (orientationString == landscape)
        return blink::WebScreenOrientationLandscapePrimary | blink::WebScreenOrientationLandscapeSecondary;

    unsigned length = 0;
    ScreenOrientationInfo* orientationMap = orientationsMap(length);
    for (unsigned i = 0; i < length; ++i) {
        if (orientationMap[i].name == orientationString)
            return orientationMap[i].orientation;
    }
    return 0;
}

ScreenOrientation::ScreenOrientation(Screen& screen)
    : DOMWindowProperty(screen.frame())
    , m_orientationLockTimer(this, &ScreenOrientation::orientationLockTimerFired)
    , m_lockedOrientations(WebScreenOrientationDefault)
{
}

void ScreenOrientation::lockOrientationAsync(blink::WebScreenOrientations orientations)
{
    if (m_lockedOrientations == orientations)
        return;
    m_lockedOrientations = orientations;
    if (!m_orientationLockTimer.isActive())
        m_orientationLockTimer.startOneShot(0);
}

void ScreenOrientation::orientationLockTimerFired(Timer<ScreenOrientation>*)
{
    if (m_lockedOrientations == WebScreenOrientationDefault)
        blink::Platform::current()->unlockOrientation();
    else
        blink::Platform::current()->lockOrientation(m_lockedOrientations);
}

const char* ScreenOrientation::supplementName()
{
    return "ScreenOrientation";
}

Document* ScreenOrientation::document() const
{
    if (!m_associatedDOMWindow || !m_associatedDOMWindow->isCurrentlyDisplayedInFrame())
        return 0;
    ASSERT(m_associatedDOMWindow->document());
    return m_associatedDOMWindow->document();
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
    if (!screenOrientation.document())
        return emptyAtom;
    ScreenOrientationController& controller = ScreenOrientationController::from(*screenOrientation.document());
    return orientationToString(controller.orientation());
}

bool ScreenOrientation::lockOrientation(Screen& screen, const Vector<String>& orientationsVector)
{
    blink::WebScreenOrientations orientations = 0;
    for (size_t i = 0; i < orientationsVector.size(); ++i) {
        blink::WebScreenOrientations currentOrientation = stringToOrientations(AtomicString(orientationsVector[i]));
        if (!currentOrientation)
            return false;
        orientations |= currentOrientation;
    }
    if (!orientations)
        return false;
    ScreenOrientation::from(screen).lockOrientationAsync(orientations);
    return true;
}

bool ScreenOrientation::lockOrientation(Screen& screen, const AtomicString& orientationString)
{
    blink::WebScreenOrientations orientations = stringToOrientations(orientationString);
    if (!orientations)
        return false;
    ScreenOrientation::from(screen).lockOrientationAsync(orientations);
    return true;
}

void ScreenOrientation::unlockOrientation(Screen& screen)
{
    ScreenOrientation::from(screen).lockOrientationAsync(WebScreenOrientationDefault);
}

} // namespace WebCore
