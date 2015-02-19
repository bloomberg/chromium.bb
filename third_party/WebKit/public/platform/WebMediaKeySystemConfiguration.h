// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebMediaKeySystemConfiguration_h
#define WebMediaKeySystemConfiguration_h

#include "public/platform/WebMediaKeySystemMediaCapability.h"
#include "public/platform/WebString.h"
#include "public/platform/WebVector.h"

namespace blink {

struct WebMediaKeySystemConfiguration {
    enum class Requirement {
        Required,
        Optional,
        NotAllowed,
    };

    WebMediaKeySystemConfiguration()
        : distinctiveIdentifier(Requirement::Optional)
        , persistentState(Requirement::Optional)
    {
    }

    WebVector<WebString> initDataTypes;
    WebVector<WebMediaKeySystemMediaCapability> audioCapabilities;
    WebVector<WebMediaKeySystemMediaCapability> videoCapabilities;
    Requirement distinctiveIdentifier;
    Requirement persistentState;
    WebVector<WebString> sessionTypes;
};

} // namespace blink

#endif // WebMediaKeySystemConfiguration_h
