// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FetchUtils_h
#define FetchUtils_h

#include "platform/PlatformExport.h"
#include "wtf/Allocator.h"
#include "wtf/Forward.h"

namespace blink {

class HTTPHeaderMap;

class PLATFORM_EXPORT FetchUtils {
  STATIC_ONLY(FetchUtils);

 public:
  static bool isSimpleMethod(const String& method);
  static bool isSimpleHeader(const AtomicString& name,
                             const AtomicString& value);
  static bool isSimpleContentType(const AtomicString& mediaType);
  static bool isSimpleRequest(const String& method, const HTTPHeaderMap&);
  static bool isForbiddenMethod(const String& method);
  static bool isUsefulMethod(const String& method) {
    return !isForbiddenMethod(method);
  }
  static bool isForbiddenHeaderName(const String& name);
  static bool isForbiddenResponseHeaderName(const String& name);
  static bool isSimpleOrForbiddenRequest(const String& method,
                                         const HTTPHeaderMap&);
  static AtomicString normalizeMethod(const AtomicString& method);
  static String normalizeHeaderValue(const String& value);

  // https://fetch.spec.whatwg.org/#ok-status aka a successful 2xx status
  // code, https://tools.ietf.org/html/rfc7231#section-6.3 . We opt to use
  // the Fetch term in naming the predicate.
  static bool isOkStatus(int status) { return status >= 200 && status < 300; }
};

}  // namespace blink

#endif
