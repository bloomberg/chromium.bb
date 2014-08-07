// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/geofencing/Geofencing.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"

namespace blink {

Geofencing::Geofencing()
{
    ScriptWrappable::init(this);
}

ScriptPromise Geofencing::registerRegion(ScriptState* scriptState, GeofencingRegion* region)
{
    return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(NotSupportedError));
}

ScriptPromise Geofencing::unregisterRegion(ScriptState* scriptState, const String& regionId)
{
    return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(NotSupportedError));
}

ScriptPromise Geofencing::getRegisteredRegions(ScriptState* scriptState) const
{
    return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(NotSupportedError));
}

} // namespace blink
