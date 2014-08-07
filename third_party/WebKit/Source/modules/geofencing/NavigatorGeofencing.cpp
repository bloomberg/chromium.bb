// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/geofencing/NavigatorGeofencing.h"

#include "core/frame/Navigator.h"
#include "modules/geofencing/Geofencing.h"

namespace blink {

NavigatorGeofencing::NavigatorGeofencing()
{
}

NavigatorGeofencing::~NavigatorGeofencing()
{
}

const char* NavigatorGeofencing::supplementName()
{
    return "NavigatorGeofencing";
}

NavigatorGeofencing& NavigatorGeofencing::from(Navigator& navigator)
{
    NavigatorGeofencing* supplement = static_cast<NavigatorGeofencing*>(WillBeHeapSupplement<Navigator>::from(navigator, supplementName()));
    if (!supplement) {
        supplement = new NavigatorGeofencing();
        provideTo(navigator, supplementName(), adoptPtrWillBeNoop(supplement));
    }
    return *supplement;
}

Geofencing* NavigatorGeofencing::geofencing(Navigator& navigator)
{
    return NavigatorGeofencing::from(navigator).geofencing();
}

Geofencing* NavigatorGeofencing::geofencing()
{
    if (!m_geofencing)
        m_geofencing = Geofencing::create();
    return m_geofencing.get();
}

void NavigatorGeofencing::trace(Visitor* visitor)
{
    visitor->trace(m_geofencing);
    WillBeHeapSupplement<Navigator>::trace(visitor);
}

} // namespace blink
