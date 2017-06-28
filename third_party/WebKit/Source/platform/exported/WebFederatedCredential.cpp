// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/WebFederatedCredential.h"

#include "platform/credentialmanager/PlatformFederatedCredential.h"

namespace blink {
WebFederatedCredential::WebFederatedCredential(
    const WebString& id,
    const WebSecurityOrigin& provider,
    const WebString& name,
    const WebURL& icon_url)
    : WebCredential(
          PlatformFederatedCredential::Create(id, provider, name, icon_url)) {}

void WebFederatedCredential::Assign(const WebFederatedCredential& other) {
  platform_credential_ = other.platform_credential_;
}

WebSecurityOrigin WebFederatedCredential::Provider() const {
  return static_cast<PlatformFederatedCredential*>(platform_credential_.Get())
      ->Provider();
}

WebString WebFederatedCredential::Name() const {
  return static_cast<PlatformFederatedCredential*>(platform_credential_.Get())
      ->Name();
}

WebURL WebFederatedCredential::IconURL() const {
  return static_cast<PlatformFederatedCredential*>(platform_credential_.Get())
      ->IconURL();
}

WebFederatedCredential::WebFederatedCredential(PlatformCredential* credential)
    : WebCredential(credential) {}

WebFederatedCredential& WebFederatedCredential::operator=(
    PlatformCredential* credential) {
  platform_credential_ = credential;
  return *this;
}

}  // namespace blink
