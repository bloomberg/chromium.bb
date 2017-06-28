// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/credentialmanager/PlatformPasswordCredential.h"

namespace blink {

PlatformPasswordCredential* PlatformPasswordCredential::Create(
    const String& id,
    const String& password,
    const String& name,
    const KURL& icon_url) {
  return new PlatformPasswordCredential(id, password, name, icon_url);
}

PlatformPasswordCredential::PlatformPasswordCredential(const String& id,
                                                       const String& password,
                                                       const String& name,
                                                       const KURL& icon_url)
    : PlatformCredential(id),
      name_(name),
      icon_url_(icon_url),
      password_(password) {
  SetType("password");
}

PlatformPasswordCredential::~PlatformPasswordCredential() {}

}  // namespace blink
