// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/cors/cors.h"

#include <memory>
#include <string>
#include <utility>

#include "net/http/http_util.h"
#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/cpp/cors/preflight_cache.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/platform/loader/cors/cors_error_string.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/network/http_header_map.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"
#include "url/gurl.h"

namespace blink {

namespace {

base::Optional<std::string> GetHeaderValue(const HTTPHeaderMap& header_map,
                                           const AtomicString& header_name) {
  if (header_map.Contains(header_name)) {
    return WebString(header_map.Get(header_name)).Latin1();
  }
  return base::nullopt;
}

network::cors::PreflightCache& GetPerThreadPreflightCache() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<network::cors::PreflightCache>,
                                  cache, ());
  return *cache;
}

base::Optional<std::string> GetOptionalHeaderValue(
    const HTTPHeaderMap& header_map,
    const AtomicString& header_name) {
  const AtomicString& result = header_map.Get(header_name);
  if (result.IsNull())
    return base::nullopt;

  return std::string(result.Ascii().data());
}

std::unique_ptr<net::HttpRequestHeaders> CreateNetHttpRequestHeaders(
    const HTTPHeaderMap& header_map) {
  std::unique_ptr<net::HttpRequestHeaders> request_headers =
      std::make_unique<net::HttpRequestHeaders>();
  for (HTTPHeaderMap::const_iterator i = header_map.begin(),
                                     end = header_map.end();
       i != end; ++i) {
    DCHECK(!i->key.IsNull());
    DCHECK(!i->value.IsNull());
    request_headers->SetHeader(std::string(i->key.Ascii().data()),
                               std::string(i->value.Ascii().data()));
  }
  return request_headers;
}

url::Origin AsUrlOrigin(const SecurityOrigin& origin) {
  // "file:" origin is treated like an opaque unique origin when
  // allow-file-access-from-files is not specified. Such origin is not
  // opaque (i.e., IsOpaque() returns false) but still serializes to
  // "null".
  return origin.ToString() == "null" ? url::Origin() : origin.ToUrlOrigin();
}

// A parser for the value of the Access-Control-Expose-Headers header.
class HTTPHeaderNameListParser {
  STACK_ALLOCATED();

 public:
  explicit HTTPHeaderNameListParser(const String& value)
      : value_(value), pos_(0) {}

  // Tries parsing |value_| expecting it to be conforming to the #field-name
  // ABNF rule defined in RFC 7230. Returns with the field-name entries stored
  // in |output| when successful. Otherwise, returns with |output| kept empty.
  //
  // |output| must be empty.
  void Parse(WebHTTPHeaderSet& output) {
    DCHECK(output.empty());

    while (true) {
      ConsumeSpaces();

      if (pos_ == value_.length() && !output.empty()) {
        output.insert(std::string());
        return;
      }

      size_t token_start = pos_;
      ConsumeTokenChars();
      size_t token_size = pos_ - token_start;
      if (token_size == 0) {
        output.clear();
        return;
      }

      const CString& name = value_.Substring(token_start, token_size).Ascii();
      output.emplace(name.data(), name.length());

      ConsumeSpaces();

      if (pos_ == value_.length())
        return;

      if (value_[pos_] == ',') {
        ++pos_;
      } else {
        output.clear();
        return;
      }
    }
  }

 private:
  // Consumes zero or more spaces (SP and HTAB) from value_.
  void ConsumeSpaces() {
    while (true) {
      if (pos_ == value_.length())
        return;

      UChar c = value_[pos_];
      if (c != ' ' && c != '\t')
        return;
      ++pos_;
    }
  }

  // Consumes zero or more tchars from value_.
  void ConsumeTokenChars() {
    while (true) {
      if (pos_ == value_.length())
        return;

      UChar c = value_[pos_];
      if (c > 0x7F || !net::HttpUtil::IsTokenChar(c))
        return;
      ++pos_;
    }
  }

  const String value_;
  size_t pos_;
};

}  // namespace

namespace cors {

base::Optional<network::CorsErrorStatus> CheckAccess(
    const KURL& response_url,
    const int response_status_code,
    const HTTPHeaderMap& response_header,
    network::mojom::FetchCredentialsMode credentials_mode,
    const SecurityOrigin& origin) {
  return network::cors::CheckAccess(
      response_url, response_status_code,
      GetHeaderValue(response_header, http_names::kAccessControlAllowOrigin),
      GetHeaderValue(response_header,
                     http_names::kAccessControlAllowCredentials),
      credentials_mode, AsUrlOrigin(origin));
}

base::Optional<network::CorsErrorStatus> CheckPreflightAccess(
    const KURL& response_url,
    const int response_status_code,
    const HTTPHeaderMap& response_header,
    network::mojom::FetchCredentialsMode actual_credentials_mode,
    const SecurityOrigin& origin) {
  return network::cors::CheckPreflightAccess(
      response_url, response_status_code,
      GetHeaderValue(response_header, http_names::kAccessControlAllowOrigin),
      GetHeaderValue(response_header,
                     http_names::kAccessControlAllowCredentials),
      actual_credentials_mode, AsUrlOrigin(origin));
}

base::Optional<network::CorsErrorStatus> CheckRedirectLocation(
    const KURL& url,
    network::mojom::FetchRequestMode request_mode,
    const SecurityOrigin* origin,
    CorsFlag cors_flag) {
  base::Optional<url::Origin> origin_to_pass;
  if (origin)
    origin_to_pass = AsUrlOrigin(*origin);

  // Blink-side implementations rewrite the origin instead of setting the
  // tainted flag.
  return network::cors::CheckRedirectLocation(
      url, request_mode, origin_to_pass, cors_flag == CorsFlag::Set, false);
}

base::Optional<network::mojom::CorsError> CheckPreflight(
    const int preflight_response_status_code) {
  return network::cors::CheckPreflight(preflight_response_status_code);
}

base::Optional<network::CorsErrorStatus> CheckExternalPreflight(
    const HTTPHeaderMap& response_header) {
  return network::cors::CheckExternalPreflight(
      GetHeaderValue(response_header, http_names::kAccessControlAllowExternal));
}

bool IsCorsEnabledRequestMode(network::mojom::FetchRequestMode request_mode) {
  return network::cors::IsCorsEnabledRequestMode(request_mode);
}

base::Optional<network::CorsErrorStatus> EnsurePreflightResultAndCacheOnSuccess(
    const HTTPHeaderMap& response_header_map,
    const String& origin,
    const KURL& request_url,
    const String& request_method,
    const HTTPHeaderMap& request_header_map,
    network::mojom::FetchCredentialsMode request_credentials_mode) {
  DCHECK(!origin.IsNull());
  DCHECK(!request_method.IsNull());

  base::Optional<network::mojom::CorsError> error;

  std::unique_ptr<network::cors::PreflightResult> result =
      network::cors::PreflightResult::Create(
          request_credentials_mode,
          GetOptionalHeaderValue(response_header_map,
                                 http_names::kAccessControlAllowMethods),
          GetOptionalHeaderValue(response_header_map,
                                 http_names::kAccessControlAllowHeaders),
          GetOptionalHeaderValue(response_header_map,
                                 http_names::kAccessControlMaxAge),
          &error);
  if (error)
    return network::CorsErrorStatus(*error);

  base::Optional<network::CorsErrorStatus> status;
  status = result->EnsureAllowedCrossOriginMethod(
      std::string(request_method.Ascii().data()));
  if (status)
    return status;

  // |is_revalidating| is not needed for blink-side CORS.
  constexpr bool is_revalidating = false;
  status = result->EnsureAllowedCrossOriginHeaders(
      *CreateNetHttpRequestHeaders(request_header_map), is_revalidating);
  if (status)
    return status;

  GetPerThreadPreflightCache().AppendEntry(std::string(origin.Ascii().data()),
                                           request_url, std::move(result));
  return base::nullopt;
}

bool CheckIfRequestCanSkipPreflight(
    const String& origin,
    const KURL& url,
    network::mojom::FetchCredentialsMode credentials_mode,
    const String& method,
    const HTTPHeaderMap& request_header_map) {
  DCHECK(!origin.IsNull());
  DCHECK(!method.IsNull());

  // |is_revalidating| is not needed for blink-side CORS.
  constexpr bool is_revalidating = false;
  return GetPerThreadPreflightCache().CheckIfRequestCanSkipPreflight(
      std::string(origin.Ascii().data()), url, credentials_mode,
      std::string(method.Ascii().data()),
      *CreateNetHttpRequestHeaders(request_header_map), is_revalidating);
}

// Keep this in sync with the identical function
// network::cors::CorsURLLoader::CalculateResponseTainting.
//
// This is the same as that function except using KURL and SecurityOrigin
// instead of GURL and url::Origin. We can't combine them because converting
// SecurityOrigin to url::Origin loses information about origins that are
// whitelisted by SecurityPolicy.
//
// This function also doesn't use a |tainted_origin| flag because Blink loaders
// mutate the origin instead of using such a flag.
network::mojom::FetchResponseType CalculateResponseTainting(
    const KURL& url,
    network::mojom::FetchRequestMode request_mode,
    const SecurityOrigin* origin,
    CorsFlag cors_flag) {
  if (url.ProtocolIsData())
    return network::mojom::FetchResponseType::kBasic;

  if (cors_flag == CorsFlag::Set) {
    DCHECK(IsCorsEnabledRequestMode(request_mode));
    return network::mojom::FetchResponseType::kCors;
  }

  if (!origin) {
    // This is actually not defined in the fetch spec, but in this case CORS
    // is disabled so no one should care this value.
    return network::mojom::FetchResponseType::kBasic;
  }

  if (request_mode == network::mojom::FetchRequestMode::kNoCors &&
      !origin->CanRequest(url)) {
    return network::mojom::FetchResponseType::kOpaque;
  }
  return network::mojom::FetchResponseType::kBasic;
}

bool CalculateCredentialsFlag(
    network::mojom::FetchCredentialsMode credentials_mode,
    network::mojom::FetchResponseType response_tainting) {
  return network::cors::CalculateCredentialsFlag(credentials_mode,
                                                 response_tainting);
}

bool IsCorsSafelistedMethod(const String& method) {
  DCHECK(!method.IsNull());
  return network::cors::IsCorsSafelistedMethod(WebString(method).Latin1());
}

bool IsCorsSafelistedContentType(const String& media_type) {
  return network::cors::IsCorsSafelistedContentType(
      WebString(media_type).Latin1());
}

bool IsNoCorsSafelistedHeaderName(const String& name) {
  DCHECK(!name.IsNull());
  return network::cors::IsNoCorsSafelistedHeaderName(WebString(name).Latin1());
}

bool IsPrivilegedNoCorsHeaderName(const String& name) {
  DCHECK(!name.IsNull());
  return network::cors::IsPrivilegedNoCorsHeaderName(WebString(name).Latin1());
}

bool IsNoCorsSafelistedHeader(const String& name, const String& value) {
  DCHECK(!name.IsNull());
  DCHECK(!value.IsNull());
  return network::cors::IsNoCorsSafelistedHeader(WebString(name).Latin1(),
                                                 WebString(value).Latin1());
}

Vector<String> CorsUnsafeRequestHeaderNames(const HTTPHeaderMap& headers) {
  net::HttpRequestHeaders::HeaderVector in;
  for (const auto& entry : headers) {
    in.push_back(net::HttpRequestHeaders::HeaderKeyValuePair(
        WebString(entry.key).Latin1(), WebString(entry.value).Latin1()));
  }

  Vector<String> header_names;
  for (const auto& name : network::cors::CorsUnsafeRequestHeaderNames(in))
    header_names.push_back(WebString::FromLatin1(name));
  return header_names;
}

bool IsForbiddenHeaderName(const String& name) {
  return network::cors::IsForbiddenHeader(WebString(name).Latin1());
}

bool ContainsOnlyCorsSafelistedHeaders(const HTTPHeaderMap& header_map) {
  Vector<String> header_names = CorsUnsafeRequestHeaderNames(header_map);
  return header_names.IsEmpty();
}

bool ContainsOnlyCorsSafelistedOrForbiddenHeaders(
    const HTTPHeaderMap& headers) {
  Vector<String> header_names;

  net::HttpRequestHeaders::HeaderVector in;
  for (const auto& entry : headers) {
    in.push_back(net::HttpRequestHeaders::HeaderKeyValuePair(
        WebString(entry.key).Latin1(), WebString(entry.value).Latin1()));
  }
  // |is_revalidating| is not needed for blink-side CORS.
  constexpr bool is_revalidating = false;
  return network::cors::CorsUnsafeNotForbiddenRequestHeaderNames(
             in, is_revalidating)
      .empty();
}

bool IsOkStatus(int status) {
  return network::cors::IsOkStatus(status);
}

bool CalculateCorsFlag(const KURL& url,
                       const SecurityOrigin* origin,
                       network::mojom::FetchRequestMode request_mode) {
  if (request_mode == network::mojom::FetchRequestMode::kNavigate ||
      request_mode == network::mojom::FetchRequestMode::kNoCors) {
    return false;
  }
  // CORS needs a proper origin (including a unique opaque origin). If the
  // request doesn't have one, CORS should not work.
  DCHECK(origin);
  return !origin->CanReadContent(url);
}

WebHTTPHeaderSet ExtractCorsExposedHeaderNamesList(
    network::mojom::FetchCredentialsMode credentials_mode,
    const ResourceResponse& response) {
  // If a response was fetched via a service worker, it will always have
  // CorsExposedHeaderNames set from the Access-Control-Expose-Headers header.
  // For requests that didn't come from a service worker, just parse the CORS
  // header.
  if (response.WasFetchedViaServiceWorker()) {
    WebHTTPHeaderSet header_set;
    for (const auto& header : response.CorsExposedHeaderNames())
      header_set.emplace(header.Ascii().data(), header.Ascii().length());
    return header_set;
  }

  WebHTTPHeaderSet header_set;
  HTTPHeaderNameListParser parser(response.HttpHeaderField(
      WebString(http_names::kAccessControlExposeHeaders)));
  parser.Parse(header_set);

  if (credentials_mode != network::mojom::FetchCredentialsMode::kInclude &&
      header_set.find("*") != header_set.end()) {
    header_set.clear();
    for (const auto& header : response.HttpHeaderFields()) {
      CString name = header.key.Ascii();
      header_set.emplace(name.data(), name.length());
    }
  }
  return header_set;
}

bool IsCorsSafelistedResponseHeader(const String& name) {
  // https://fetch.spec.whatwg.org/#cors-safelisted-response-header-name
  // TODO(dcheng): Consider using a flat_set here with a transparent comparator.
  DEFINE_THREAD_SAFE_STATIC_LOCAL(WebHTTPHeaderSet,
                                  allowed_cross_origin_response_headers,
                                  ({
                                      "cache-control",
                                      "content-language",
                                      "content-length",
                                      "content-type",
                                      "expires",
                                      "last-modified",
                                      "pragma",
                                  }));
  return allowed_cross_origin_response_headers.find(name.Ascii().data()) !=
         allowed_cross_origin_response_headers.end();
}

// In the spec, https://fetch.spec.whatwg.org/#ref-for-concept-request-mode,
// No-CORS mode is highly discouraged from using it for new features. Only
// legacy usages for backward compatibility are allowed except for well-designed
// usages over the fetch API.
bool IsNoCorsAllowedContext(mojom::RequestContextType context) {
  switch (context) {
    case mojom::RequestContextType::AUDIO:
    case mojom::RequestContextType::FAVICON:
    case mojom::RequestContextType::FETCH:
    case mojom::RequestContextType::IMAGE:
    case mojom::RequestContextType::OBJECT:
    case mojom::RequestContextType::PLUGIN:
    case mojom::RequestContextType::SCRIPT:
    case mojom::RequestContextType::SHARED_WORKER:
    case mojom::RequestContextType::VIDEO:
    case mojom::RequestContextType::WORKER:
      return true;
    default:
      return false;
  }
}

}  // namespace cors

}  // namespace blink
