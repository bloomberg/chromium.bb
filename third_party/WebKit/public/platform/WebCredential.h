// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebCredential_h
#define WebCredential_h

#include "public/platform/WebCommon.h"
#include "public/platform/WebPrivatePtr.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"

#include <memory>

namespace blink {

class PlatformCredential;

class WebCredential {
 public:
  BLINK_PLATFORM_EXPORT WebCredential(const WebString& id);
  BLINK_PLATFORM_EXPORT WebCredential(const WebCredential&);
  virtual ~WebCredential() { Reset(); }

  BLINK_PLATFORM_EXPORT void Assign(const WebCredential&);
  BLINK_PLATFORM_EXPORT void Reset();

  BLINK_PLATFORM_EXPORT WebString Id() const;
  BLINK_PLATFORM_EXPORT WebString GetType() const;

  BLINK_PLATFORM_EXPORT bool IsPasswordCredential() const;
  BLINK_PLATFORM_EXPORT bool IsFederatedCredential() const;

  // TODO(mkwst): Drop this once Chromium is updated. https://crbug.com/494880
  BLINK_PLATFORM_EXPORT bool IsLocalCredential() const {
    return IsPasswordCredential();
  }

#if INSIDE_BLINK
  BLINK_PLATFORM_EXPORT static std::unique_ptr<WebCredential> Create(
      PlatformCredential*);
  BLINK_PLATFORM_EXPORT WebCredential& operator=(PlatformCredential*);
  BLINK_PLATFORM_EXPORT PlatformCredential* GetPlatformCredential() const {
    return platform_credential_.Get();
  }
#endif

 protected:
#if INSIDE_BLINK
  BLINK_PLATFORM_EXPORT WebCredential(PlatformCredential*);
#endif

  WebPrivatePtr<PlatformCredential> platform_credential_;
};

}  // namespace blink

#endif  // WebCredential_h
