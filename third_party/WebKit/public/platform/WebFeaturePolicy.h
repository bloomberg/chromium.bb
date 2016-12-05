// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebFeaturePolicy_h
#define WebFeaturePolicy_h

#include "WebSecurityOrigin.h"
#include "WebString.h"
#include "WebVector.h"

namespace blink {

struct WebFeaturePolicy {
  struct ParsedWhitelist {
    ParsedWhitelist() : matchesAllOrigins(false) {}
    WebString featureName;
    bool matchesAllOrigins;
    WebVector<WebSecurityOrigin> origins;
  };
};

using WebParsedFeaturePolicy = WebVector<WebFeaturePolicy::ParsedWhitelist>;

}  // namespace blink

#endif  // WebFeaturePolicy_h
