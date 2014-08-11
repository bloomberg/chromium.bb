// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebFederatedCredential_h
#define WebFederatedCredential_h

#include "WebCommon.h"
#include "WebCredential.h"
#include "WebString.h"

namespace blink {

class WebFederatedCredential : public WebCredential {
public:
    BLINK_PLATFORM_EXPORT WebFederatedCredential(const WebString& id, const WebString& name, const WebString& avatarURL, const WebString& federation);

    BLINK_PLATFORM_EXPORT void assign(const WebFederatedCredential&);

    BLINK_PLATFORM_EXPORT WebString federation() const;
};

} // namespace blink

#endif // WebFederatedCredential_h


