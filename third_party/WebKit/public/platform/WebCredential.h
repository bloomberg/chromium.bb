// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebCredential_h
#define WebCredential_h

#include "public/platform/WebCommon.h"
#include "public/platform/WebPrivatePtr.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"

namespace blink {

class PlatformCredential;

class WebCredential {
public:
    BLINK_PLATFORM_EXPORT WebCredential(const WebString& id, const WebString& name, const WebURL& avatarURL);
    ~WebCredential() { reset(); }

    BLINK_PLATFORM_EXPORT void assign(const WebCredential&);
    BLINK_PLATFORM_EXPORT void reset();

    BLINK_PLATFORM_EXPORT WebString id() const;
    BLINK_PLATFORM_EXPORT WebString name() const;
    BLINK_PLATFORM_EXPORT WebURL avatarURL() const;

protected:
    BLINK_PLATFORM_EXPORT explicit WebCredential(PlatformCredential*);
    WebPrivatePtr<PlatformCredential> m_platformCredential;
};

} // namespace blink

#endif // WebCredential_h
