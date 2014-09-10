// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/geofencing/Geofencing.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "modules/geofencing/CircularGeofencingRegion.h"
#include "modules/geofencing/GeofencingError.h"
#include "modules/geofencing/GeofencingRegion.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCircularGeofencingRegion.h"
#include "public/platform/WebGeofencingProvider.h"
#include "public/platform/WebGeofencingRegistration.h"

namespace blink {

namespace {

// For CallbackPromiseAdapter to convert a WebVector of regions to a HeapVector.
class RegionArray {
public:
    typedef blink::WebVector<blink::WebGeofencingRegistration> WebType;
    static HeapVector<Member<GeofencingRegion> > take(ScriptPromiseResolver* resolver, WebType* regionsRaw)
    {
        OwnPtr<WebType> webRegions = adoptPtr(regionsRaw);
        HeapVector<Member<GeofencingRegion> > regions;
        for (size_t i = 0; i < webRegions->size(); ++i)
            regions.append(CircularGeofencingRegion::create((*webRegions)[i].id, (*webRegions)[i].region));
        return regions;
    }

    static void dispose(WebType* regionsRaw)
    {
        delete regionsRaw;
    }

private:
    RegionArray();
};

} // namespace

Geofencing::Geofencing()
{
}

ScriptPromise Geofencing::registerRegion(ScriptState* scriptState, GeofencingRegion* region)
{
    WebGeofencingProvider* provider = Platform::current()->geofencingProvider();
    if (!provider)
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(NotSupportedError));

    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    // FIXME: somehow pass a reference to the current serviceworker to the provider.
    provider->registerRegion(region->id(), toCircularGeofencingRegion(region)->webRegion(), new CallbackPromiseAdapter<void, GeofencingError>(resolver));
    return promise;
}

ScriptPromise Geofencing::unregisterRegion(ScriptState* scriptState, const String& regionId)
{
    WebGeofencingProvider* provider = Platform::current()->geofencingProvider();
    if (!provider)
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(NotSupportedError));

    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    // FIXME: somehow pass a reference to the current serviceworker to the provider.
    provider->unregisterRegion(regionId, new CallbackPromiseAdapter<void, GeofencingError>(resolver));
    return promise;
}

ScriptPromise Geofencing::getRegisteredRegions(ScriptState* scriptState) const
{
    WebGeofencingProvider* provider = Platform::current()->geofencingProvider();
    if (!provider)
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(NotSupportedError));

    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    // FIXME: somehow pass a reference to the current serviceworker to the provider.
    provider->getRegisteredRegions(new CallbackPromiseAdapter<RegionArray, GeofencingError>(resolver));
    return promise;
}

} // namespace blink
