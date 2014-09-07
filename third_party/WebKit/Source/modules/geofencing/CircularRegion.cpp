// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/geofencing/CircularRegion.h"

#include "bindings/core/v8/Dictionary.h"

namespace blink {

CircularRegionInit::CircularRegionInit(const Dictionary& init)
    : latitude(0)
    , longitude(0)
    , radius(0)
{
    DictionaryHelper::get(init, "id", id);
    DictionaryHelper::get(init, "latitude", latitude);
    DictionaryHelper::get(init, "longitude", longitude);
    DictionaryHelper::get(init, "radius", radius);
}

CircularRegion* CircularRegion::create(const Dictionary& init)
{
    return new CircularRegion(CircularRegionInit(init));
}

CircularRegion::CircularRegion(const CircularRegionInit& init)
    : GeofencingRegion(init.id)
    , m_latitude(init.latitude)
    , m_longitude(init.longitude)
    , m_radius(init.radius)
{
}

} // namespace blink
