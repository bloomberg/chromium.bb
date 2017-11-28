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

#include "public/platform/WebCORSPreflightResultCache.h"

#include <memory>
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "platform/loader/fetch/FetchUtils.h"
#include "platform/loader/fetch/ResourceResponse.h"
#include "platform/network/http_names.h"
#include "platform/wtf/StdLibExtras.h"
#include "platform/wtf/ThreadSpecific.h"
#include "platform/wtf/Time.h"
#include "public/platform/WebCORS.h"

namespace blink {

namespace {

// These values are at the discretion of the user agent.

constexpr TimeDelta kDefaultPreflightCacheTimeout = TimeDelta::FromSeconds(5);

// Should be short enough to minimize the risk of using a poisoned cache after
// switching to a secure network.
constexpr TimeDelta kMaxPreflightCacheTimeout = TimeDelta::FromSeconds(600);

bool ParseAccessControlMaxAge(const String& string, TimeDelta& expiry_delta) {
  // FIXME: this will not do the correct thing for a number starting with a '+'
  bool ok = false;
  expiry_delta = TimeDelta::FromSeconds(string.ToUIntStrict(&ok));
  return ok;
}

template <class SetType>
void AddToAccessControlAllowList(const std::string& string,
                                 unsigned start,
                                 unsigned end,
                                 SetType& set) {
  if (string.empty())
    return;

  // Skip white space from start.
  while (start <= end && IsSpaceOrNewline(string.at(start)))
    ++start;

  // only white space
  if (start > end)
    return;

  // Skip white space from end.
  while (end && IsSpaceOrNewline(string.at(end)))
    --end;

  set.insert(string.substr(start, end - start + 1));
}

template <class SetType>
bool ParseAccessControlAllowList(const std::string& string, SetType& set) {
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

WebCORSPreflightResultCacheItem::WebCORSPreflightResultCacheItem(
    network::mojom::FetchCredentialsMode credentials_mode,
    base::TickClock* clock)
    : credentials_(credentials_mode ==
                   network::mojom::FetchCredentialsMode::kInclude),
      clock_(clock) {}

// static
std::unique_ptr<WebCORSPreflightResultCacheItem>
WebCORSPreflightResultCacheItem::Create(
    const network::mojom::FetchCredentialsMode credentials_mode,
    const WebHTTPHeaderMap& response_header,
    WebString& error_description,
    base::TickClock* clock) {
  std::unique_ptr<WebCORSPreflightResultCacheItem> item =
      base::WrapUnique(new WebCORSPreflightResultCacheItem(
          credentials_mode,
          clock ? clock : base::DefaultTickClock::GetInstance()));

  if (!item->Parse(response_header, error_description))
    return nullptr;

  return item;
}

bool WebCORSPreflightResultCacheItem::Parse(
    const WebHTTPHeaderMap& response_header,
    WebString& error_description) {
  methods_.clear();

  const HTTPHeaderMap& response_header_map = response_header.GetHTTPHeaderMap();

  if (!ParseAccessControlAllowList(
          response_header_map.Get(HTTPNames::Access_Control_Allow_Methods)
              .Ascii()
              .data(),
          methods_)) {
    error_description =
        "Cannot parse Access-Control-Allow-Methods response header field in "
        "preflight response.";
    return false;
  }

  headers_.clear();
  if (!ParseAccessControlAllowList(
          response_header_map.Get(HTTPNames::Access_Control_Allow_Headers)
              .Ascii()
              .data(),
          headers_)) {
    error_description =
        "Cannot parse Access-Control-Allow-Headers response header field in "
        "preflight response.";
    return false;
  }

  TimeDelta expiry_delta;
  if (ParseAccessControlMaxAge(
          response_header_map.Get(HTTPNames::Access_Control_Max_Age),
          expiry_delta)) {
    if (expiry_delta > kMaxPreflightCacheTimeout)
      expiry_delta = kMaxPreflightCacheTimeout;
  } else {
    expiry_delta = kDefaultPreflightCacheTimeout;
  }

  absolute_expiry_time_ = clock_->NowTicks() + expiry_delta;

  return true;
}

bool WebCORSPreflightResultCacheItem::AllowsCrossOriginMethod(
    const WebString& method,
    WebString& error_description) const {
  if (methods_.find(method.Ascii().data()) != methods_.end() ||
      FetchUtils::IsCORSSafelistedMethod(method)) {
    return true;
  }

  if (!credentials_ && methods_.find("*") != methods_.end())
    return true;

  error_description = WebString::FromASCII("Method " + method.Ascii() +
                                           " is not allowed by "
                                           "Access-Control-Allow-Methods "
                                           "in preflight response.");

  return false;
}

bool WebCORSPreflightResultCacheItem::AllowsCrossOriginHeaders(
    const WebHTTPHeaderMap& request_headers,
    WebString& error_description) const {
  if (!credentials_ && headers_.find("*") != headers_.end())
    return true;

  for (const auto& header : request_headers.GetHTTPHeaderMap()) {
    if (headers_.find(header.key.Ascii().data()) == headers_.end() &&
        !FetchUtils::IsCORSSafelistedHeader(header.key, header.value) &&
        !FetchUtils::IsForbiddenHeaderName(header.key)) {
      error_description = String::Format(
          "Request header field %s is not allowed by "
          "Access-Control-Allow-Headers in preflight response.",
          header.key.GetString().Utf8().data());
      return false;
    }
  }
  return true;
}

bool WebCORSPreflightResultCacheItem::AllowsRequest(
    network::mojom::FetchCredentialsMode credentials_mode,
    const WebString& method,
    const WebHTTPHeaderMap& request_headers) const {
  WebString ignored_explanation;

  if (absolute_expiry_time_ < clock_->NowTicks())
    return false;
  if (!credentials_ &&
      credentials_mode == network::mojom::FetchCredentialsMode::kInclude) {
    return false;
  }
  if (!AllowsCrossOriginMethod(method, ignored_explanation))
    return false;
  if (!AllowsCrossOriginHeaders(request_headers, ignored_explanation))
    return false;
  return true;
}

WebCORSPreflightResultCache& WebCORSPreflightResultCache::Shared() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<WebCORSPreflightResultCache>,
                                  cache, ());
  return *cache;
}

WebCORSPreflightResultCache::WebCORSPreflightResultCache() = default;
WebCORSPreflightResultCache::~WebCORSPreflightResultCache() = default;

void WebCORSPreflightResultCache::AppendEntry(
    const WebString& web_origin,
    const WebURL& web_url,
    std::unique_ptr<WebCORSPreflightResultCacheItem> preflight_result) {
  std::string url(web_url.GetString().Ascii());
  std::string origin(web_origin.Ascii());

  preflight_hash_map_[origin][url] = std::move(preflight_result);
}

bool WebCORSPreflightResultCache::CanSkipPreflight(
    const WebString& web_origin,
    const WebURL& web_url,
    network::mojom::FetchCredentialsMode credentials_mode,
    const WebString& method,
    const WebHTTPHeaderMap& request_headers) {
  std::string origin(web_origin.Ascii());
  std::string url(web_url.GetString().Ascii());

  // either origin or url not in cache:
  if (preflight_hash_map_.find(origin) == preflight_hash_map_.end() ||
      preflight_hash_map_[origin].find(url) ==
          preflight_hash_map_[origin].end()) {
    return false;
  }

  // both origin and url in cache -> check if still valid and if sufficient to
  // skip redirect:
  if (preflight_hash_map_[origin][url]->AllowsRequest(credentials_mode, method,
                                                      request_headers)) {
    return true;
  }

  // Either cache entry is stale or not sufficient. Remove item from cache:
  preflight_hash_map_[origin].erase(url);
  if (preflight_hash_map_[origin].empty())
    preflight_hash_map_.erase(origin);

  return false;
}

}  // namespace blink
