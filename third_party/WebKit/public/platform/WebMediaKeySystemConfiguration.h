// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebMediaKeySystemConfiguration_h
#define WebMediaKeySystemConfiguration_h

#include "public/platform/WebEncryptedMediaTypes.h"
#include "public/platform/WebMediaKeySystemMediaCapability.h"
#include "public/platform/WebVector.h"

namespace blink {

struct WebMediaKeySystemConfiguration {
    enum class Requirement {
        Required,
        Optional,
        NotAllowed,
    };

    // In MediaKeySystemConfiguration, sequence members may be not-present, and
    // requestMediaKeySystemAccess() distinguishes non-present from empty. Here
    // we have a flag for each one to signal that state.
    bool hasInitDataTypes = false;
    WebVector<WebEncryptedMediaInitDataType> initDataTypes;

    bool hasAudioCapabilities = false;
    WebVector<WebMediaKeySystemMediaCapability> audioCapabilities;

    bool hasVideoCapabilities = false;
    WebVector<WebMediaKeySystemMediaCapability> videoCapabilities;

    Requirement distinctiveIdentifier = Requirement::Optional;
    Requirement persistentState = Requirement::Optional;

    bool hasSessionTypes = false;
    WebVector<WebEncryptedMediaSessionType> sessionTypes;
};

} // namespace blink

#endif // WebMediaKeySystemConfiguration_h
