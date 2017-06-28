// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PlatformPasswordCredential_h
#define PlatformPasswordCredential_h

#include "platform/credentialmanager/PlatformCredential.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class PLATFORM_EXPORT PlatformPasswordCredential final
    : public PlatformCredential {
  WTF_MAKE_NONCOPYABLE(PlatformPasswordCredential);

 public:
  static PlatformPasswordCredential* Create(const String& id,
                                            const String& password,
                                            const String& name,
                                            const KURL& icon_url);
  ~PlatformPasswordCredential() override;

  const String& Password() const { return password_; }

  bool IsPassword() override { return true; }
  const String& Name() const { return name_; }
  const KURL& IconURL() const { return icon_url_; }

 private:
  PlatformPasswordCredential(const String& id,
                             const String& password,
                             const String& name,
                             const KURL& icon_url);
  String name_;
  KURL icon_url_;
  String password_;
};

}  // namespace blink

#endif  // PlatformPasswordCredential_h
