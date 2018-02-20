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
#include "net/http/http_request_headers.h"
#include "platform/loader/cors/CORS.h"
#include "platform/loader/cors/CORSErrorString.h"
#include "platform/loader/fetch/FetchUtils.h"
#include "platform/loader/fetch/ResourceResponse.h"
#include "platform/network/http_names.h"
#include "platform/wtf/StdLibExtras.h"
#include "platform/wtf/ThreadSpecific.h"
#include "platform/wtf/Time.h"
#include "public/platform/WebCORS.h"

namespace blink {

namespace {

base::Optional<std::string> GetOptionalHeaderValue(
    const WebHTTPHeaderMap& header_map,
    const AtomicString& header_name) {
  const AtomicString& result = header_map.Get(header_name);
  if (result.IsNull())
    return base::nullopt;

  return std::string(result.Ascii().data(), result.Ascii().length());
}

std::unique_ptr<net::HttpRequestHeaders> CreateNetHttpRequestHeaders(
    const WebHTTPHeaderMap& header_map) {
  std::unique_ptr<net::HttpRequestHeaders> request_headers =
      std::make_unique<net::HttpRequestHeaders>();
  for (const auto& header : header_map.GetHTTPHeaderMap()) {
    // TODO(toyoshim): Use CHECK() for a while to ensure that these assumptions
    // are correct. Should be changed to DCHECK before the next branch cut.
    CHECK(!header.key.IsNull());
    CHECK(!header.value.IsNull());
    request_headers->SetHeader(
        std::string(header.key.Ascii().data(), header.key.Ascii().length()),
        std::string(header.value.Ascii().data(),
                    header.value.Ascii().length()));
  }
  return request_headers;
}

}  // namespace

WebCORSPreflightResultCache& WebCORSPreflightResultCache::Shared() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<WebCORSPreflightResultCache>,
                                  cache, ());
  return *cache;
}

WebCORSPreflightResultCache::WebCORSPreflightResultCache() = default;
WebCORSPreflightResultCache::~WebCORSPreflightResultCache() = default;

bool WebCORSPreflightResultCache::EnsureResultAndMayAppendEntry(
    const WebHTTPHeaderMap& response_header_map,
    const WebString& origin,
    const WebURL& request_url,
    const WebString& request_method,
    const WebHTTPHeaderMap& request_header_map,
    network::mojom::FetchCredentialsMode request_credentials_mode,
    WebString* error_description) {
  DCHECK(error_description);

  base::Optional<network::mojom::CORSError> error;

  std::unique_ptr<network::cors::PreflightResult> result =
      network::cors::PreflightResult::Create(
          request_credentials_mode,
          GetOptionalHeaderValue(response_header_map,
                                 HTTPNames::Access_Control_Allow_Methods),
          GetOptionalHeaderValue(response_header_map,
                                 HTTPNames::Access_Control_Allow_Headers),
          GetOptionalHeaderValue(response_header_map,
                                 HTTPNames::Access_Control_Max_Age),
          &error);
  if (error) {
    *error_description = CORS::GetErrorString(
        CORS::ErrorParameter::CreateForPreflightResponseCheck(*error,
                                                              String()));
    return false;
  }

  DCHECK(!request_method.IsNull());
  error = result->EnsureAllowedCrossOriginMethod(std::string(
      request_method.Ascii().data(), request_method.Ascii().length()));
  if (error) {
    *error_description = CORS::GetErrorString(
        CORS::ErrorParameter::CreateForPreflightResponseCheck(*error,
                                                              request_method));
    return false;
  }

  std::string detected_error_header;
  error = result->EnsureAllowedCrossOriginHeaders(
      *CreateNetHttpRequestHeaders(request_header_map), &detected_error_header);
  if (error) {
    *error_description = CORS::GetErrorString(
        CORS::ErrorParameter::CreateForPreflightResponseCheck(
            *error, String(detected_error_header.data(),
                           detected_error_header.length())));
    return false;
  }

  DCHECK(!error);
  AppendEntry(origin, request_url, std::move(result));
  return true;
}

void WebCORSPreflightResultCache::AppendEntry(
    const WebString& web_origin,
    const WebURL& web_url,
    std::unique_ptr<network::cors::PreflightResult> preflight_result) {
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
  DCHECK(!method.IsNull());
  if (preflight_hash_map_[origin][url]->EnsureAllowedRequest(
          credentials_mode,
          std::string(method.Ascii().data(), method.Ascii().length()),
          *CreateNetHttpRequestHeaders(request_headers))) {
    return true;
  }

  // Either cache entry is stale or not sufficient. Remove item from cache:
  preflight_hash_map_[origin].erase(url);
  if (preflight_hash_map_[origin].empty())
    preflight_hash_map_.erase(origin);

  return false;
}

}  // namespace blink
