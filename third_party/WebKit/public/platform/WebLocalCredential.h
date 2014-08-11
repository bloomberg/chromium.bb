// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebLocalCredential_h
#define WebLocalCredential_h

#include "WebCommon.h"
#include "WebCredential.h"
#include "WebString.h"

namespace blink {

class WebLocalCredential : public WebCredential {
public:
    BLINK_PLATFORM_EXPORT WebLocalCredential(const WebString& id, const WebString& name, const WebString& avatarURL, const WebString& password);

    BLINK_PLATFORM_EXPORT void assign(const WebLocalCredential&);

    BLINK_PLATFORM_EXPORT WebString password() const;
};

} // namespace blink

#endif // WebLocalCredential_h



