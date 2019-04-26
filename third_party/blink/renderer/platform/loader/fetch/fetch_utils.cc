// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/fetch_utils.h"

#include "services/network/public/cpp/cors/cors.h"
#include "third_party/blink/renderer/platform/network/http_header_map.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"

namespace blink {

namespace {

bool IsHTTPWhitespace(UChar chr) {
  return chr == ' ' || chr == '\n' || chr == '\t' || chr == '\r';
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

const char* FetchUtils::GetDestinationFromContext(
    mojom::RequestContextType context) {
  switch (context) {
    case mojom::RequestContextType::UNSPECIFIED:
    case mojom::RequestContextType::BEACON:
    case mojom::RequestContextType::DOWNLOAD:
    case mojom::RequestContextType::EVENT_SOURCE:
    case mojom::RequestContextType::FETCH:
    case mojom::RequestContextType::PING:
    case mojom::RequestContextType::XML_HTTP_REQUEST:
    case mojom::RequestContextType::SUBRESOURCE:
    case mojom::RequestContextType::PREFETCH:
      return "";
    case mojom::RequestContextType::CSP_REPORT:
      return "report";
    case mojom::RequestContextType::AUDIO:
      return "audio";
    case mojom::RequestContextType::EMBED:
      return "embed";
    case mojom::RequestContextType::FONT:
      return "font";
    case mojom::RequestContextType::FRAME:
    case mojom::RequestContextType::HYPERLINK:
    case mojom::RequestContextType::IFRAME:
    case mojom::RequestContextType::LOCATION:
    case mojom::RequestContextType::FORM:
      return "document";
    case mojom::RequestContextType::IMAGE:
    case mojom::RequestContextType::FAVICON:
    case mojom::RequestContextType::IMAGE_SET:
      return "image";
    case mojom::RequestContextType::MANIFEST:
      return "manifest";
    case mojom::RequestContextType::OBJECT:
      return "object";
    case mojom::RequestContextType::SCRIPT:
      return "script";
    case mojom::RequestContextType::SERVICE_WORKER:
      return "serviceworker";
    case mojom::RequestContextType::SHARED_WORKER:
      return "sharedworker";
    case mojom::RequestContextType::STYLE:
      return "style";
    case mojom::RequestContextType::TRACK:
      return "track";
    case mojom::RequestContextType::VIDEO:
      return "video";
    case mojom::RequestContextType::WORKER:
      return "worker";
    case mojom::RequestContextType::XSLT:
      return "xslt";
    case mojom::RequestContextType::IMPORT:
    case mojom::RequestContextType::INTERNAL:
    case mojom::RequestContextType::PLUGIN:
      return "unknown";
  }
  NOTREACHED();
  return "";
}

}  // namespace blink
