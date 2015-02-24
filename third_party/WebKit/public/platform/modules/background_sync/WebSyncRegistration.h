// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebSyncRegistration_h
#define WebSyncRegistration_h

#include "public/platform/WebPrivatePtr.h"
#include "public/platform/WebString.h"

namespace blink {

struct WebSyncRegistrationOptions {
    enum NetworkType {
        NetworkTypeAny = 0,
        NetworkTypeOffline,
        NetworkTypeOnline,
        NetworkTypeNonMobile,
        NetworkTypeLast = NetworkTypeNonMobile
    };

    WebSyncRegistrationOptions(NetworkType networkType, bool allowOnBattery, bool idleRequired)
        : networkType(networkType)
        , allowOnBattery(allowOnBattery)
        , idleRequired(idleRequired)
    {
    }

    NetworkType networkType;
    bool allowOnBattery;
    bool idleRequired;
};


struct WebSyncRegistration {
    WebSyncRegistration(const WebString& registrationId, const WebSyncRegistrationOptions& options)
        : id(registrationId)
        , options(options)
    {
    }

    WebString id;
    WebSyncRegistrationOptions options;
};

} // namespace blink

#endif // WebSyncRegistration_h
