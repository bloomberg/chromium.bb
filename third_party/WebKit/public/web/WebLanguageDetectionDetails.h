// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebLanguageDetectionDetails_h
#define WebLanguageDetectionDetails_h

#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"

namespace blink {

class WebDocument;

struct WebLanguageDetectionDetails {
  WebURL url;
  WebString contentLanguage;
  WebString htmlLanguage;
  bool hasNoTranslateMeta = false;

  BLINK_EXPORT static WebLanguageDetectionDetails
  collectLanguageDetectionDetails(const WebDocument&);
};

}  // namespace blink

#endif  // WebLanguageDetectionDetails_h
