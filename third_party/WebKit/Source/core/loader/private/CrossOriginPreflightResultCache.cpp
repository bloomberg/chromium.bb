/*
 * Copyright (C) 2008, 2009 Apple Inc. All Rights Reserved.
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
 *
 */

#include "core/loader/private/CrossOriginPreflightResultCache.h"

#include <memory>
#include "platform/HTTPNames.h"
#include "platform/loader/fetch/FetchUtils.h"
#include "platform/loader/fetch/ResourceResponse.h"
#include "platform/wtf/CurrentTime.h"
#include "platform/wtf/StdLibExtras.h"

namespace blink {

namespace {

// These values are at the discretion of the user agent.

constexpr unsigned kDefaultPreflightCacheTimeoutSeconds = 5;

// Should be short enough to minimize the risk of using a poisoned cache after
// switching to a secure network.
constexpr unsigned kMaxPreflightCacheTimeoutSeconds = 600;

bool ParseAccessControlMaxAge(const String& string, unsigned& expiry_delta) {
  // FIXME: this will not do the correct thing for a number starting with a '+'
  bool ok = false;
  expiry_delta = string.ToUIntStrict(&ok);
  return ok;
}

template <class HashType>
void AddToAccessControlAllowList(const String& string,
                                 unsigned start,
                                 unsigned end,
                                 HashSet<String, HashType>& set) {
  StringImpl* string_impl = string.Impl();
  if (!string_impl)
    return;

  // Skip white space from start.
  while (start <= end && IsSpaceOrNewline((*string_impl)[start]))
    ++start;

  // only white space
  if (start > end)
    return;

  // Skip white space from end.
  while (end && IsSpaceOrNewline((*string_impl)[end]))
    --end;

  set.insert(string.Substring(start, end - start + 1));
}

template <class HashType>
bool ParseAccessControlAllowList(const String& string,
                                 HashSet<String, HashType>& set) {
  unsigned start = 0;
  size_t end;
  while ((end = string.find(',', start)) != kNotFound) {
    if (start != end)
      AddToAccessControlAllowList(string, start, end - 1, set);
    start = end + 1;
  }
  if (start != string.length())
    AddToAccessControlAllowList(string, start, string.length() - 1, set);

  return true;
}

}  // namespace

CrossOriginPreflightResultCacheItem::CrossOriginPreflightResultCacheItem(
    WebURLRequest::FetchCredentialsMode credentials_mode)
    : absolute_expiry_time_(0),
      credentials_(
          FetchUtils::ShouldTreatCredentialsModeAsInclude(credentials_mode)) {}

bool CrossOriginPreflightResultCacheItem::Parse(
    const ResourceResponse& response,
    String& error_description) {
  methods_.clear();
  if (!ParseAccessControlAllowList(
          response.HttpHeaderField(HTTPNames::Access_Control_Allow_Methods),
          methods_)) {
    error_description =
        "Cannot parse Access-Control-Allow-Methods response header field in "
        "preflight response.";
    return false;
  }

  headers_.clear();
  if (!ParseAccessControlAllowList(
          response.HttpHeaderField(HTTPNames::Access_Control_Allow_Headers),
          headers_)) {
    error_description =
        "Cannot parse Access-Control-Allow-Headers response header field in "
        "preflight response.";
    return false;
  }

  unsigned expiry_delta;
  if (ParseAccessControlMaxAge(
          response.HttpHeaderField(HTTPNames::Access_Control_Max_Age),
          expiry_delta)) {
    if (expiry_delta > kMaxPreflightCacheTimeoutSeconds)
      expiry_delta = kMaxPreflightCacheTimeoutSeconds;
  } else {
    expiry_delta = kDefaultPreflightCacheTimeoutSeconds;
  }

  absolute_expiry_time_ = CurrentTime() + expiry_delta;
  return true;
}

bool CrossOriginPreflightResultCacheItem::AllowsCrossOriginMethod(
    const String& method,
    String& error_description) const {
  if (methods_.Contains(method) || FetchUtils::IsCORSSafelistedMethod(method))
    return true;

  error_description =
      "Method " + method +
      " is not allowed by Access-Control-Allow-Methods in preflight response.";
  return false;
}

bool CrossOriginPreflightResultCacheItem::AllowsCrossOriginHeaders(
    const HTTPHeaderMap& request_headers,
    String& error_description) const {
  for (const auto& header : request_headers) {
    if (!headers_.Contains(header.key) &&
        !FetchUtils::IsCORSSafelistedHeader(header.key, header.value) &&
        !FetchUtils::IsForbiddenHeaderName(header.key)) {
      error_description = "Request header field " + header.key.GetString() +
                          " is not allowed by Access-Control-Allow-Headers in "
                          "preflight response.";
      return false;
    }
  }
  return true;
}

bool CrossOriginPreflightResultCacheItem::AllowsRequest(
    WebURLRequest::FetchCredentialsMode credentials_mode,
    const String& method,
    const HTTPHeaderMap& request_headers) const {
  String ignored_explanation;
  if (absolute_expiry_time_ < CurrentTime())
    return false;
  if (!credentials_ &&
      FetchUtils::ShouldTreatCredentialsModeAsInclude(credentials_mode))
    return false;
  if (!AllowsCrossOriginMethod(method, ignored_explanation))
    return false;
  if (!AllowsCrossOriginHeaders(request_headers, ignored_explanation))
    return false;
  return true;
}

CrossOriginPreflightResultCache& CrossOriginPreflightResultCache::Shared() {
  DEFINE_STATIC_LOCAL(CrossOriginPreflightResultCache, cache, ());
  DCHECK(IsMainThread());
  return cache;
}

void CrossOriginPreflightResultCache::AppendEntry(
    const String& origin,
    const KURL& url,
    std::unique_ptr<CrossOriginPreflightResultCacheItem> preflight_result) {
  DCHECK(IsMainThread());
  preflight_hash_map_.Set(std::make_pair(origin, url),
                          std::move(preflight_result));
}

bool CrossOriginPreflightResultCache::CanSkipPreflight(
    const String& origin,
    const KURL& url,
    WebURLRequest::FetchCredentialsMode credentials_mode,
    const String& method,
    const HTTPHeaderMap& request_headers) {
  DCHECK(IsMainThread());
  CrossOriginPreflightResultHashMap::iterator cache_it =
      preflight_hash_map_.find(std::make_pair(origin, url));
  if (cache_it == preflight_hash_map_.end())
    return false;

  if (cache_it->value->AllowsRequest(credentials_mode, method, request_headers))
    return true;

  preflight_hash_map_.erase(cache_it);
  return false;
}

}  // namespace blink
