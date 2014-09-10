// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebGeofencingProvider_h
#define WebGeofencingProvider_h

#include "public/platform/WebCallbacks.h"
#include "public/platform/WebVector.h"

namespace blink {

struct WebCircularGeofencingRegion;
struct WebGeofencingError;
struct WebGeofencingRegistration;
class WebString;

typedef WebCallbacks<void, WebGeofencingError> WebGeofencingCallbacks;
typedef WebCallbacks<WebVector<WebGeofencingRegistration>, WebGeofencingError> WebGeofencingRegionsCallbacks;

class WebGeofencingProvider {
public:
    virtual ~WebGeofencingProvider() { }

    // Registers a region.
    // Ownership of the WebGeofencingCallbacks is transferred to the client.
    virtual void registerRegion(const WebString& regionId, const WebCircularGeofencingRegion&, WebGeofencingCallbacks*) = 0;

    // Unregisters a region.
    // Ownership of the WebGeofencingCallbacks is transferred to the client.
    virtual void unregisterRegion(const WebString& regionId, WebGeofencingCallbacks*) = 0;

    // Returns all the currently registered regions.
    // Ownership of the WebGeofencingRegionsCallbacks is transferred to the client.
    virtual void getRegisteredRegions(WebGeofencingRegionsCallbacks*) = 0;
};

} // namespace blink

#endif // WebGeofencingProvider_h
