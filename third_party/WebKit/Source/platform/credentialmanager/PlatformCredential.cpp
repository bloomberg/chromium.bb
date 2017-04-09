// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/credentialmanager/PlatformCredential.h"

namespace blink {

PlatformCredential* PlatformCredential::Create(const String& id,
                                               const String& name,
                                               const KURL& icon_url) {
  return new PlatformCredential(id, name, icon_url);
}

PlatformCredential::PlatformCredential(const String& id,
                                       const String& name,
                                       const KURL& icon_url)
    : id_(id), name_(name), icon_url_(icon_url), type_("credential") {}

PlatformCredential::~PlatformCredential() {}

}  // namespace blink
