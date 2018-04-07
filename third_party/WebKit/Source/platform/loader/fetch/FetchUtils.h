// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_FETCH_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_FETCH_UTILS_H_

#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class HTTPHeaderMap;

class PLATFORM_EXPORT FetchUtils {
  STATIC_ONLY(FetchUtils);

 public:
  static bool IsForbiddenMethod(const String& method);
  static bool IsForbiddenHeaderName(const String& name);
  static bool IsForbiddenResponseHeaderName(const String& name);
  static AtomicString NormalizeMethod(const AtomicString& method);
  static String NormalizeHeaderValue(const String& value);
  static bool ContainsOnlyCORSSafelistedHeaders(const HTTPHeaderMap&);
  static bool ContainsOnlyCORSSafelistedOrForbiddenHeaders(
      const HTTPHeaderMap&);

  // https://fetch.spec.whatwg.org/#ok-status aka a successful 2xx status
  // code, https://tools.ietf.org/html/rfc7231#section-6.3 . We opt to use
  // the Fetch term in naming the predicate.
  static bool IsOkStatus(int status) { return status >= 200 && status < 300; }
};

}  // namespace blink

#endif
