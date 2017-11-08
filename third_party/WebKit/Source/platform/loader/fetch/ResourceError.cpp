/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/loader/fetch/ResourceError.h"

#include "net/base/net_errors.h"
#include "platform/loader/fetch/ResourceRequest.h"
#include "public/platform/Platform.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLError.h"

namespace blink {

namespace {
constexpr char kThrottledErrorDescription[] =
    "Request throttled. Visit http://dev.chromium.org/throttling for more "
    "information.";
}  // namespace

ResourceError ResourceError::CancelledError(const KURL& url) {
  return ResourceError(Domain::kNet, net::ERR_ABORTED, url);
}

ResourceError ResourceError::CancelledDueToAccessCheckError(
    const KURL& url,
    ResourceRequestBlockedReason blocked_reason) {
  ResourceError error = CancelledError(url);
  error.SetIsAccessCheck(true);
  if (blocked_reason == ResourceRequestBlockedReason::kSubresourceFilter)
    error.SetShouldCollapseInitiator(true);
  return error;
}

ResourceError ResourceError::CancelledDueToAccessCheckError(
    const KURL& url,
    ResourceRequestBlockedReason blocked_reason,
    const String& localized_description) {
  ResourceError error = CancelledDueToAccessCheckError(url, blocked_reason);
  error.localized_description_ = localized_description;
  return error;
}

ResourceError ResourceError::CacheMissError(const KURL& url) {
  return ResourceError(Domain::kNet, net::ERR_CACHE_MISS, url);
}

ResourceError ResourceError::TimeoutError(const KURL& url) {
  return ResourceError(Domain::kNet, net::ERR_TIMED_OUT, url);
}

ResourceError ResourceError::Failure(const KURL& url) {
  return ResourceError(Domain::kNet, net::ERR_FAILED, url);
}

ResourceError::ResourceError(Domain domain, int error_code, const KURL& url)
    : domain_(domain), error_code_(error_code), failing_url_(url) {
  DCHECK_NE(error_code_, 0);
  InitializeDescription();
}

ResourceError::ResourceError(const WebURLError& error)
    : domain_(error.domain()),
      error_code_(error.reason()),
      failing_url_(error.url()),
      is_access_check_(error.is_web_security_violation()),
      stale_copy_in_cache_(error.has_copy_in_cache()) {
  DCHECK_NE(error_code_, 0);
  InitializeDescription();
}

ResourceError ResourceError::Copy() const {
  ResourceError error_copy(domain_, error_code_, failing_url_.Copy());
  error_copy.stale_copy_in_cache_ = stale_copy_in_cache_;
  error_copy.localized_description_ = localized_description_.IsolatedCopy();
  error_copy.is_access_check_ = is_access_check_;
  return error_copy;
}

ResourceError::operator WebURLError() const {
  return WebURLError(domain_, error_code_,
                     stale_copy_in_cache_ ? WebURLError::HasCopyInCache::kTrue
                                          : WebURLError::HasCopyInCache::kFalse,
                     is_access_check_
                         ? WebURLError::IsWebSecurityViolation::kTrue
                         : WebURLError::IsWebSecurityViolation::kFalse,
                     failing_url_);
}

bool ResourceError::Compare(const ResourceError& a, const ResourceError& b) {
  if (a.GetDomain() != b.GetDomain())
    return false;

  if (a.ErrorCode() != b.ErrorCode())
    return false;

  if (a.FailingURL() != b.FailingURL())
    return false;

  if (a.LocalizedDescription() != b.LocalizedDescription())
    return false;

  if (a.IsAccessCheck() != b.IsAccessCheck())
    return false;

  if (a.StaleCopyInCache() != b.StaleCopyInCache())
    return false;

  return true;
}

bool ResourceError::IsTimeout() const {
  return domain_ == Domain::kNet && error_code_ == net::ERR_TIMED_OUT;
}

bool ResourceError::IsCancellation() const {
  return domain_ == Domain::kNet && error_code_ == net::ERR_ABORTED;
}

bool ResourceError::IsCacheMiss() const {
  return domain_ == Domain::kNet && error_code_ == net::ERR_CACHE_MISS;
}

void ResourceError::InitializeDescription() {
  if (domain_ == Domain::kNet) {
    if (error_code_ == net::ERR_TEMPORARILY_THROTTLED) {
      localized_description_ = WebString::FromASCII(kThrottledErrorDescription);
    } else {
      localized_description_ =
          WebString::FromASCII(net::ErrorToString(error_code_));
    }
  }
}

}  // namespace blink
