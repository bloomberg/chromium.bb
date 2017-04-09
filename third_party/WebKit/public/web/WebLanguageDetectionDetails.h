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
  WebString content_language;
  WebString html_language;
  bool has_no_translate_meta = false;

  BLINK_EXPORT static WebLanguageDetectionDetails
  CollectLanguageDetectionDetails(const WebDocument&);
};

}  // namespace blink

#endif  // WebLanguageDetectionDetails_h
