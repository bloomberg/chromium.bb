// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebPasswordCredential_h
#define WebPasswordCredential_h

#include "public/platform/WebCommon.h"
#include "public/platform/WebCredential.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"

namespace blink {

class WebPasswordCredential : public WebCredential {
 public:
  BLINK_PLATFORM_EXPORT WebPasswordCredential(const WebString& id,
                                              const WebString& password,
                                              const WebString& name,
                                              const WebURL& icon_url);

  BLINK_PLATFORM_EXPORT void Assign(const WebPasswordCredential&);

  BLINK_PLATFORM_EXPORT WebString Password() const;
  BLINK_PLATFORM_EXPORT WebString Name() const;
  BLINK_PLATFORM_EXPORT WebURL IconURL() const;

#if INSIDE_BLINK
  BLINK_PLATFORM_EXPORT WebPasswordCredential(PlatformCredential*);
  BLINK_PLATFORM_EXPORT WebPasswordCredential& operator=(PlatformCredential*);
#endif
};

}  // namespace blink

#endif  // WebPasswordCredential_h
