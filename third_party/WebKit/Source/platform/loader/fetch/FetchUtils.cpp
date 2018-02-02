// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/loader/fetch/FetchUtils.h"

#include "platform/loader/cors/CORS.h"
#include "platform/network/HTTPHeaderMap.h"
#include "platform/network/HTTPParsers.h"
#include "platform/network/http_names.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/Threading.h"
#include "platform/wtf/text/AtomicString.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

namespace {

bool IsHTTPWhitespace(UChar chr) {
  return chr == ' ' || chr == '\n' || chr == '\t' || chr == '\r';
}

class ForbiddenHeaderNames {
  WTF_MAKE_NONCOPYABLE(ForbiddenHeaderNames);
  USING_FAST_MALLOC(ForbiddenHeaderNames);

 public:
  bool Has(const String& name) const {
    return fixed_names_.Contains(name) ||
           name.StartsWithIgnoringASCIICase(proxy_header_prefix_) ||
           name.StartsWithIgnoringASCIICase(sec_header_prefix_);
  }

  static const ForbiddenHeaderNames& Get();

 private:
  ForbiddenHeaderNames();

  String proxy_header_prefix_;
  String sec_header_prefix_;
  HashSet<String, CaseFoldingHash> fixed_names_;
};

ForbiddenHeaderNames::ForbiddenHeaderNames()
    : proxy_header_prefix_("proxy-"), sec_header_prefix_("sec-") {
  fixed_names_ = {
      "accept-charset",
      "accept-encoding",
      "access-control-request-headers",
      "access-control-request-method",
      "connection",
      "content-length",
      "cookie",
      "cookie2",
      "date",
      "dnt",
      "expect",
      "host",
      "keep-alive",
      "origin",
      "referer",
      "te",
      "trailer",
      "transfer-encoding",
      "upgrade",
      "user-agent",
      "via",
  };
}

const ForbiddenHeaderNames& ForbiddenHeaderNames::Get() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(const ForbiddenHeaderNames, instance, ());
  return instance;
}

}  // namespace

bool FetchUtils::IsForbiddenMethod(const String& method) {
  // http://fetch.spec.whatwg.org/#forbidden-method
  // "A forbidden method is a method that is a byte case-insensitive match"
  //  for one of `CONNECT`, `TRACE`, and `TRACK`."
  return EqualIgnoringASCIICase(method, "TRACE") ||
         EqualIgnoringASCIICase(method, "TRACK") ||
         EqualIgnoringASCIICase(method, "CONNECT");
}

bool FetchUtils::IsForbiddenHeaderName(const String& name) {
  // http://fetch.spec.whatwg.org/#forbidden-header-name
  // "A forbidden header name is a header names that is one of:
  //   `Accept-Charset`, `Accept-Encoding`, `Access-Control-Request-Headers`,
  //   `Access-Control-Request-Method`, `Connection`,
  //   `Content-Length, Cookie`, `Cookie2`, `Date`, `DNT`, `Expect`, `Host`,
  //   `Keep-Alive`, `Origin`, `Referer`, `TE`, `Trailer`,
  //   `Transfer-Encoding`, `Upgrade`, `User-Agent`, `Via`
  // or starts with `Proxy-` or `Sec-` (including when it is just `Proxy-` or
  // `Sec-`)."

  return ForbiddenHeaderNames::Get().Has(name);
}

bool FetchUtils::IsForbiddenResponseHeaderName(const String& name) {
  // http://fetch.spec.whatwg.org/#forbidden-response-header-name
  // "A forbidden response header name is a header name that is one of:
  // `Set-Cookie`, `Set-Cookie2`"

  return EqualIgnoringASCIICase(name, "set-cookie") ||
         EqualIgnoringASCIICase(name, "set-cookie2");
}

AtomicString FetchUtils::NormalizeMethod(const AtomicString& method) {
  // https://fetch.spec.whatwg.org/#concept-method-normalize

  // We place GET and POST first because they are more commonly used than
  // others.
  const char* const kMethods[] = {
      "GET", "POST", "DELETE", "HEAD", "OPTIONS", "PUT",
  };

  for (auto* const known : kMethods) {
    if (EqualIgnoringASCIICase(method, known)) {
      // Don't bother allocating a new string if it's already all
      // uppercase.
      return method == known ? method : known;
    }
  }
  return method;
}

String FetchUtils::NormalizeHeaderValue(const String& value) {
  // https://fetch.spec.whatwg.org/#concept-header-value-normalize
  // Strip leading and trailing whitespace from header value.
  // HTTP whitespace bytes are 0x09, 0x0A, 0x0D, and 0x20.

  return value.StripWhiteSpace(IsHTTPWhitespace);
}

bool FetchUtils::ContainsOnlyCORSSafelistedHeaders(
    const HTTPHeaderMap& header_map) {
  for (const auto& header : header_map) {
    if (!CORS::IsCORSSafelistedHeader(header.key, header.value))
      return false;
  }
  return true;
}

bool FetchUtils::ContainsOnlyCORSSafelistedOrForbiddenHeaders(
    const HTTPHeaderMap& header_map) {
  for (const auto& header : header_map) {
    if (!CORS::IsCORSSafelistedHeader(header.key, header.value) &&
        !IsForbiddenHeaderName(header.key))
      return false;
  }
  return true;
}

}  // namespace blink
