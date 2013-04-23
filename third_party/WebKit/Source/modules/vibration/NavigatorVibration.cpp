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

#if ENABLE(VIBRATION)

#include "core/dom/ExceptionCode.h"
#include "core/page/Frame.h"
#include "core/page/Navigator.h"
#include "core/page/Page.h"
#include "modules/vibration/Vibration.h"
#include "wtf/Uint32Array.h"

namespace WebCore {

NavigatorVibration::NavigatorVibration()
{
}

NavigatorVibration::~NavigatorVibration()
{
}

void NavigatorVibration::vibrate(Navigator* navigator, unsigned time, ExceptionCode& ec)
{
    if (!navigator->frame()->page())
        return;

    if (navigator->frame()->page()->visibilityState() == PageVisibilityStateHidden)
        return;

    if (!Vibration::isActive(navigator->frame()->page())) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }

    Vibration::from(navigator->frame()->page())->vibrate(time);
}

void NavigatorVibration::vibrate(Navigator* navigator, const VibrationPattern& pattern, ExceptionCode& ec)
{
    if (!navigator->frame()->page())
        return;

    if (navigator->frame()->page()->visibilityState() == PageVisibilityStateHidden)
        return;

    if (!Vibration::isActive(navigator->frame()->page())) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }

    Vibration::from(navigator->frame()->page())->vibrate(pattern);
}

} // namespace WebCore

#endif // ENABLE(VIBRATION)

