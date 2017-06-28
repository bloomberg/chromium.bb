// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/credentialmanager/PlatformFederatedCredential.h"

namespace blink {

PlatformFederatedCredential* PlatformFederatedCredential::Create(
    const String& id,
    RefPtr<SecurityOrigin> provider,
    const String& name,
    const KURL& icon_url) {
  return new PlatformFederatedCredential(id, std::move(provider), name,
                                         icon_url);
}

PlatformFederatedCredential::PlatformFederatedCredential(
    const String& id,
    RefPtr<SecurityOrigin> provider,
    const String& name,
    const KURL& icon_url)
    : PlatformCredential(id),
      name_(name),
      icon_url_(icon_url),
      provider_(std::move(provider)) {
  SetType("federated");
}

PlatformFederatedCredential::~PlatformFederatedCredential() {}

}  // namespace blink
