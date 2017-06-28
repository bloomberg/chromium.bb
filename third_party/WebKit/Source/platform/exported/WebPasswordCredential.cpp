// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/WebPasswordCredential.h"

#include "platform/credentialmanager/PlatformPasswordCredential.h"

namespace blink {
WebPasswordCredential::WebPasswordCredential(const WebString& id,
                                             const WebString& password,
                                             const WebString& name,
                                             const WebURL& icon_url)
    : WebCredential(
          PlatformPasswordCredential::Create(id, password, name, icon_url)) {}

void WebPasswordCredential::Assign(const WebPasswordCredential& other) {
  platform_credential_ = other.platform_credential_;
}

WebString WebPasswordCredential::Password() const {
  return static_cast<PlatformPasswordCredential*>(platform_credential_.Get())
      ->Password();
}

WebString WebPasswordCredential::Name() const {
  return static_cast<PlatformPasswordCredential*>(platform_credential_.Get())
      ->Name();
}

WebURL WebPasswordCredential::IconURL() const {
  return static_cast<PlatformPasswordCredential*>(platform_credential_.Get())
      ->IconURL();
}

WebPasswordCredential::WebPasswordCredential(PlatformCredential* credential)
    : WebCredential(credential) {}

WebPasswordCredential& WebPasswordCredential::operator=(
    PlatformCredential* credential) {
  platform_credential_ = credential;
  return *this;
}

}  // namespace blink
