// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebSyncRegistration_h
#define WebSyncRegistration_h

#include "public/platform/WebPrivatePtr.h"
#include "public/platform/WebString.h"

namespace blink {

struct WebSyncRegistration {
    enum NetworkType {
        NetworkTypeAny = 0,
        NetworkTypeOffline,
        NetworkTypeOnline,
        NetworkTypeNonMobile,
        NetworkTypeLast = NetworkTypeNonMobile
    };

    WebSyncRegistration(const WebString& registrationId, unsigned long minDelayMs,
        unsigned long maxDelayMs, unsigned long minPeriodMs, NetworkType minRequiredNetwork,
        bool allowOnBattery, bool idleRequired)
        : id(registrationId)
        , minDelayMs(minDelayMs)
        , maxDelayMs(maxDelayMs)
        , minPeriodMs(minPeriodMs)
        , minRequiredNetwork(minRequiredNetwork)
        , allowOnBattery(allowOnBattery)
        , idleRequired(idleRequired)
    {
    }

    WebString id;

    /* Minimum delay before sync event (or first sync event, if periodic,) in
     * milliseconds. */
    unsigned long minDelayMs;

    /* Maximum delay before sync event (or first sync event, if periodic,) in
     * milliseconds. 0 means no maximum delay. If this value is greater than 0,
     * then it should not be less than minDelayMs for the registration to be
     * meaningful.
     */
    unsigned long maxDelayMs;

    /* Minimum time between periodic sync events, in milliseconds. A 0 value
     * here means that the event is a one-shot (not periodic.)
     */
    unsigned long minPeriodMs;

    NetworkType minRequiredNetwork;
    bool allowOnBattery;
    bool idleRequired;
};

} // namespace blink

#endif // WebSyncRegistration_h
