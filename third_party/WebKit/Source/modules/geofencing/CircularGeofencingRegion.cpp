// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/geofencing/CircularGeofencingRegion.h"

#include "bindings/core/v8/Dictionary.h"
#include "public/platform/WebString.h"

namespace blink {

CircularGeofencingRegion* CircularGeofencingRegion::create(const Dictionary& dictionary)
{
    String id;
    DictionaryHelper::get(dictionary, "id", id);
    WebCircularGeofencingRegion region;
    DictionaryHelper::get(dictionary, "latitude", region.latitude);
    DictionaryHelper::get(dictionary, "longitude", region.longitude);
    DictionaryHelper::get(dictionary, "radius", region.radius);
    return new CircularGeofencingRegion(id, region);
}

CircularGeofencingRegion* CircularGeofencingRegion::create(const WebString& id, const WebCircularGeofencingRegion& region)
{
    return new CircularGeofencingRegion(id, region);
}

CircularGeofencingRegion::CircularGeofencingRegion(const String& id, const WebCircularGeofencingRegion& region)
    : GeofencingRegion(id)
    , m_webRegion(region)
{
}

WebCircularGeofencingRegion CircularGeofencingRegion::webRegion() const
{
    return m_webRegion;
}

} // namespace blink
