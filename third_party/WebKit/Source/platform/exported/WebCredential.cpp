// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/WebCredential.h"

#include "platform/credentialmanager/PlatformCredential.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/WebFederatedCredential.h"
#include "public/platform/WebPasswordCredential.h"

namespace blink {

std::unique_ptr<WebCredential> WebCredential::Create(
    PlatformCredential* credential) {
  if (credential->IsPassword()) {
    return WTF::MakeUnique<WebPasswordCredential>(credential);
  }

  if (credential->IsFederated()) {
    return WTF::MakeUnique<WebFederatedCredential>(credential);
  }

  NOTREACHED();
  return nullptr;
}

WebCredential::WebCredential(const WebString& id)
    : platform_credential_(PlatformCredential::Create(id)) {}

WebCredential::WebCredential(const WebCredential& other) {
  Assign(other);
}

void WebCredential::Assign(const WebCredential& other) {
  platform_credential_ = other.platform_credential_;
}

WebCredential::WebCredential(PlatformCredential* credential)
    : platform_credential_(credential) {}

WebCredential& WebCredential::operator=(PlatformCredential* credential) {
  platform_credential_ = credential;
  return *this;
}

void WebCredential::Reset() {
  platform_credential_.Reset();
}

WebString WebCredential::Id() const {
  return platform_credential_->Id();
}

WebString WebCredential::GetType() const {
  return platform_credential_->GetType();
}

bool WebCredential::IsPasswordCredential() const {
  return platform_credential_->IsPassword();
}

bool WebCredential::IsFederatedCredential() const {
  return platform_credential_->IsFederated();
}

}  // namespace blink
