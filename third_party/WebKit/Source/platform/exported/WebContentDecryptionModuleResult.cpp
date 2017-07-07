// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/WebContentDecryptionModuleResult.h"

#include "platform/ContentDecryptionModuleResult.h"

namespace blink {

void WebContentDecryptionModuleResult::Complete() {
  impl_->Complete();
  Reset();
}

void WebContentDecryptionModuleResult::CompleteWithContentDecryptionModule(
    WebContentDecryptionModule* cdm) {
  impl_->CompleteWithContentDecryptionModule(cdm);
  Reset();
}

void WebContentDecryptionModuleResult::CompleteWithSession(
    SessionStatus status) {
  impl_->CompleteWithSession(status);
  Reset();
}

void WebContentDecryptionModuleResult::CompleteWithKeyStatus(
    WebEncryptedMediaKeyInformation::KeyStatus key_status) {
  impl_->CompleteWithKeyStatus(key_status);
  Reset();
}

void WebContentDecryptionModuleResult::CompleteWithError(
    WebContentDecryptionModuleException exception,
    unsigned long system_code,
    const WebString& error_message) {
  impl_->CompleteWithError(exception, system_code, error_message);
  Reset();
}

WebContentDecryptionModuleResult::WebContentDecryptionModuleResult(
    ContentDecryptionModuleResult* impl)
    : impl_(impl) {
  DCHECK(impl_.Get());
}

void WebContentDecryptionModuleResult::Reset() {
  impl_.Reset();
}

void WebContentDecryptionModuleResult::Assign(
    const WebContentDecryptionModuleResult& o) {
  impl_ = o.impl_;
}

}  // namespace blink
