// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebFeaturePolicy_h
#define WebFeaturePolicy_h

#include "WebSecurityOrigin.h"
#include "WebString.h"
#include "WebVector.h"

namespace blink {

struct WebParsedFeaturePolicyDeclaration {
  WebParsedFeaturePolicyDeclaration() : matchesAllOrigins(false) {}
  WebString featureName;
  bool matchesAllOrigins;
  WebVector<WebSecurityOrigin> origins;
};

// Used in Blink code to represent parsed headers. Used for IPC between renderer
// and browser.
using WebParsedFeaturePolicyHeader =
    WebVector<WebParsedFeaturePolicyDeclaration>;

}  // namespace blink

#endif  // WebFeaturePolicy_h
