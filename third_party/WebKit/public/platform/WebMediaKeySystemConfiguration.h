// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebMediaKeySystemConfiguration_h
#define WebMediaKeySystemConfiguration_h

#include "public/platform/WebEncryptedMediaTypes.h"
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

    // FIXME: Temporary methods until |initDataTypes| and |sessionTypes|
    // can be converted to be WebVector<enum>.
    BLINK_PLATFORM_EXPORT WebVector<WebEncryptedMediaInitDataType> getInitDataTypes() const;
    BLINK_PLATFORM_EXPORT void setInitDataTypes(const WebVector<WebEncryptedMediaInitDataType>&);
    BLINK_PLATFORM_EXPORT WebVector<WebEncryptedMediaSessionType> getSessionTypes() const;
    BLINK_PLATFORM_EXPORT void setSessionTypes(const WebVector<WebEncryptedMediaSessionType>&);
};

} // namespace blink

#endif // WebMediaKeySystemConfiguration_h
