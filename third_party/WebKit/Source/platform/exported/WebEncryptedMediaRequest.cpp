// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/WebEncryptedMediaRequest.h"

#include "platform/EncryptedMediaRequest.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/WebMediaKeySystemConfiguration.h"
#include "public/platform/WebSecurityOrigin.h"
#include "public/platform/WebString.h"
#include "public/platform/WebVector.h"

namespace blink {

WebEncryptedMediaRequest::WebEncryptedMediaRequest(
    const WebEncryptedMediaRequest& request) {
  Assign(request);
}

WebEncryptedMediaRequest::WebEncryptedMediaRequest(
    EncryptedMediaRequest* request)
    : private_(request) {}

WebEncryptedMediaRequest::~WebEncryptedMediaRequest() {
  Reset();
}

WebString WebEncryptedMediaRequest::KeySystem() const {
  return private_->KeySystem();
}

const WebVector<WebMediaKeySystemConfiguration>&
WebEncryptedMediaRequest::SupportedConfigurations() const {
  return private_->SupportedConfigurations();
}

WebSecurityOrigin WebEncryptedMediaRequest::GetSecurityOrigin() const {
  return WebSecurityOrigin(private_->GetSecurityOrigin());
}

void WebEncryptedMediaRequest::RequestSucceeded(
    WebContentDecryptionModuleAccess* access) {
  private_->RequestSucceeded(access);
}

void WebEncryptedMediaRequest::RequestNotSupported(
    const WebString& error_message) {
  private_->RequestNotSupported(error_message);
}

void WebEncryptedMediaRequest::Assign(const WebEncryptedMediaRequest& other) {
  private_ = other.private_;
}

void WebEncryptedMediaRequest::Reset() {
  private_.Reset();
}

}  // namespace blink
