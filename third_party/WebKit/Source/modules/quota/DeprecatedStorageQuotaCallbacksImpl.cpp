/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/quota/DeprecatedStorageQuotaCallbacksImpl.h"

#include "modules/quota/DOMError.h"

namespace blink {

DeprecatedStorageQuotaCallbacksImpl::DeprecatedStorageQuotaCallbacksImpl(
    StorageUsageCallback* usage_callback,
    StorageErrorCallback* error_callback)
    : usage_callback_(usage_callback), error_callback_(error_callback) {}

DeprecatedStorageQuotaCallbacksImpl::DeprecatedStorageQuotaCallbacksImpl(
    StorageQuotaCallback* quota_callback,
    StorageErrorCallback* error_callback)
    : quota_callback_(quota_callback), error_callback_(error_callback) {}

DeprecatedStorageQuotaCallbacksImpl::~DeprecatedStorageQuotaCallbacksImpl() {}

void DeprecatedStorageQuotaCallbacksImpl::Trace(blink::Visitor* visitor) {
  visitor->Trace(usage_callback_);
  visitor->Trace(quota_callback_);
  visitor->Trace(error_callback_);
  StorageQuotaCallbacks::Trace(visitor);
}

void DeprecatedStorageQuotaCallbacksImpl::DidQueryStorageUsageAndQuota(
    unsigned long long usage_in_bytes,
    unsigned long long quota_in_bytes) {
  if (usage_callback_)
    usage_callback_->handleEvent(usage_in_bytes, quota_in_bytes);
}

void DeprecatedStorageQuotaCallbacksImpl::DidGrantStorageQuota(
    unsigned long long usage_in_bytes,
    unsigned long long granted_quota_in_bytes) {
  if (quota_callback_)
    quota_callback_->handleEvent(granted_quota_in_bytes);
}

void DeprecatedStorageQuotaCallbacksImpl::DidFail(WebStorageQuotaError error) {
  if (error_callback_)
    error_callback_->handleEvent(
        DOMError::Create(static_cast<ExceptionCode>(error)));
}

}  // namespace blink
