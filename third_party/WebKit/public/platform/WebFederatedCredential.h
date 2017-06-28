// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebFederatedCredential_h
#define WebFederatedCredential_h

#include "public/platform/WebCommon.h"
#include "public/platform/WebCredential.h"
#include "public/platform/WebSecurityOrigin.h"
#include "public/platform/WebString.h"

namespace blink {

class WebFederatedCredential : public WebCredential {
 public:
  BLINK_PLATFORM_EXPORT WebFederatedCredential(
      const WebString& id,
      const WebSecurityOrigin& federation,
      const WebString& name,
      const WebURL& icon_url);

  BLINK_PLATFORM_EXPORT void Assign(const WebFederatedCredential&);
  BLINK_PLATFORM_EXPORT WebSecurityOrigin Provider() const;
  BLINK_PLATFORM_EXPORT WebString Name() const;
  BLINK_PLATFORM_EXPORT WebURL IconURL() const;

#if INSIDE_BLINK
  BLINK_PLATFORM_EXPORT WebFederatedCredential(PlatformCredential*);
  BLINK_PLATFORM_EXPORT WebFederatedCredential& operator=(PlatformCredential*);
#endif
};

}  // namespace blink

#endif  // WebFederatedCredential_h
