/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/modules/quota/dom_window_quota.h"

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/modules/quota/deprecated_storage_info.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

DOMWindowQuota::DOMWindowQuota(LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window) {}

const char DOMWindowQuota::kSupplementName[] = "DOMWindowQuota";

// static
DOMWindowQuota& DOMWindowQuota::From(LocalDOMWindow& window) {
  DOMWindowQuota* supplement =
      Supplement<LocalDOMWindow>::From<DOMWindowQuota>(window);
  if (!supplement) {
    supplement = MakeGarbageCollected<DOMWindowQuota>(window);
    ProvideTo(window, supplement);
  }
  return *supplement;
}

// static
DeprecatedStorageInfo* DOMWindowQuota::webkitStorageInfo(
    LocalDOMWindow& window) {
  return DOMWindowQuota::From(window).webkitStorageInfo();
}

DeprecatedStorageInfo* DOMWindowQuota::webkitStorageInfo() const {
  if (!storage_info_)
    storage_info_ = MakeGarbageCollected<DeprecatedStorageInfo>();

  // Record metrics for usage in third-party contexts.
  GetSupplementable()->CountUseOnlyInCrossSiteIframe(
      WebFeature::kPrefixedStorageInfoThirdPartyContext);

  return storage_info_.Get();
}

void DOMWindowQuota::Trace(Visitor* visitor) const {
  visitor->Trace(storage_info_);
  Supplement<LocalDOMWindow>::Trace(visitor);
}

}  // namespace blink
