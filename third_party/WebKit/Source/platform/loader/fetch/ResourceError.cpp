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
  return WebURLError(url, false, net::ERR_ABORTED);
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
  return WebURLError(url, false, net::ERR_CACHE_MISS);
}

ResourceError ResourceError::TimeoutError(const KURL& url) {
  return WebURLError(url, false, net::ERR_TIMED_OUT);
}

ResourceError ResourceError::Copy() const {
  ResourceError error_copy;
  error_copy.domain_ = domain_;
  error_copy.error_code_ = error_code_;
  error_copy.failing_url_ = failing_url_.Copy();
  error_copy.localized_description_ = localized_description_.IsolatedCopy();
  error_copy.is_null_ = is_null_;
  error_copy.is_access_check_ = is_access_check_;
  error_copy.was_ignored_by_handler_ = was_ignored_by_handler_;
  return error_copy;
}

bool ResourceError::Compare(const ResourceError& a, const ResourceError& b) {
  if (a.IsNull() && b.IsNull())
    return true;

  if (a.IsNull() || b.IsNull())
    return false;

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

  if (a.WasIgnoredByHandler() != b.WasIgnoredByHandler())
    return false;

  return true;
}

void ResourceError::InitializeWebURLError(WebURLError* error,
                                          const WebURL& url,
                                          bool stale_copy_in_cache,
                                          int reason) {
  error->domain = Domain::kNet;
  error->reason = reason;
  error->stale_copy_in_cache = stale_copy_in_cache;
  error->unreachable_url = url;
  if (reason == net::ERR_TEMPORARILY_THROTTLED) {
    error->localized_description =
        WebString::FromASCII(kThrottledErrorDescription);
  } else {
    error->localized_description =
        WebString::FromASCII(net::ErrorToString(reason));
  }
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

}  // namespace blink
