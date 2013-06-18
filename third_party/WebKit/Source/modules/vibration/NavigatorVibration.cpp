/*
 *  Copyright (C) 2012 Samsung Electronics
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "modules/vibration/NavigatorVibration.h"

#include "core/page/Frame.h"
#include "core/page/Page.h"
#include "public/platform/Platform.h"

namespace WebCore {

// Maximum duration of a vibration is 10 seconds.
const unsigned kVibrationDurationMax = 10000;

// Maximum number of entries in a vibration pattern.
const unsigned kVibrationPatternLengthMax = 99;

NavigatorVibration::NavigatorVibration()
    : m_timerStart(this, &NavigatorVibration::timerStartFired)
    , m_timerStop(this, &NavigatorVibration::timerStopFired)
    , m_isVibrating(false)
{
}

NavigatorVibration::~NavigatorVibration()
{
    if (m_isVibrating)
        cancelVibration();
}

bool NavigatorVibration::vibrate(const VibrationPattern& pattern)
{
    size_t length = pattern.size();

    // If the pattern is too long then abort.
    if (length > kVibrationPatternLengthMax)
        return false;

    // If any pattern entry is too long then abort.
    for (size_t i = 0; i < length; ++i) {
        if (pattern[i] > kVibrationDurationMax)
            return false;
    }

    // Cancelling clears the pattern so do this before setting the new pattern.
    if (m_isVibrating)
        cancelVibration();

    // If the last item in the pattern is a pause then discard it.
    if (length && !(length % 2)) {
        VibrationPattern tempPattern = pattern;
        tempPattern.removeLast();
        m_pattern = tempPattern;
    } else {
        m_pattern = pattern;
    }

    if (m_timerStart.isActive())
        m_timerStart.stop();

    if (!m_pattern.size())
        return true;

    if (m_pattern.size() == 1 && !m_pattern[0]) {
        m_pattern.clear();
        return true;
    }

    m_timerStart.startOneShot(0);
    return true;
}

void NavigatorVibration::cancelVibration()
{
    m_pattern.clear();
    if (m_isVibrating) {
        WebKit::Platform::current()->cancelVibration();
        m_isVibrating = false;
        m_timerStop.stop();
    }
}

void NavigatorVibration::suspendVibration()
{
    if (!m_isVibrating)
        return;

    m_pattern.insert(0, m_timerStop.nextFireInterval());
    m_timerStop.stop();
    cancelVibration();
}

void NavigatorVibration::resumeVibration()
{
    ASSERT(!m_timerStart.isActive());

    m_timerStart.startOneShot(0);
}

void NavigatorVibration::timerStartFired(Timer<NavigatorVibration>* timer)
{
    ASSERT_UNUSED(timer, timer == &m_timerStart);

    if (m_pattern.size()) {
        m_isVibrating = true;
        WebKit::Platform::current()->vibrate(m_pattern[0]);
        m_timerStop.startOneShot(m_pattern[0] / 1000.0);
        m_pattern.remove(0);
    }
}

void NavigatorVibration::timerStopFired(Timer<NavigatorVibration>* timer)
{
    ASSERT_UNUSED(timer, timer == &m_timerStop);

    m_isVibrating = false;

    if (m_pattern.size()) {
        m_timerStart.startOneShot(m_pattern[0] / 1000.0);
        m_pattern.remove(0);
    }
}

bool NavigatorVibration::vibrate(Navigator* navigator, unsigned time)
{
    VibrationPattern pattern;
    pattern.append(time);
    return NavigatorVibration::vibrate(navigator, pattern);
}

bool NavigatorVibration::vibrate(Navigator* navigator, const VibrationPattern& pattern)
{
    if (!navigator->frame()->page())
        return false;

    if (navigator->frame()->page()->visibilityState() != PageVisibilityStateVisible)
        return false;

    return NavigatorVibration::from(navigator)->vibrate(pattern);
}

NavigatorVibration* NavigatorVibration::from(Navigator* navigator)
{
    NavigatorVibration* navigatorVibration = static_cast<NavigatorVibration*>(Supplement<Navigator>::from(navigator, supplementName()));
    if (!navigatorVibration) {
        navigatorVibration = new NavigatorVibration();
        Supplement<Navigator>::provideTo(navigator, supplementName(), adoptPtr(navigatorVibration));
    }
    return navigatorVibration;
}

const char* NavigatorVibration::supplementName()
{
    return "NavigatorVibration";
}

} // namespace WebCore
