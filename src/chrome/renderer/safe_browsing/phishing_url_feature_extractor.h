// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// PhishingUrlFeatureExtractor handles computing URL-based features for
// the client-side phishing detection model.  These include tokens in the
// host and path, features pertaining to host length, and IP addresses.

#ifndef CHROME_RENDERER_SAFE_BROWSING_PHISHING_URL_FEATURE_EXTRACTOR_H_
#define CHROME_RENDERER_SAFE_BROWSING_PHISHING_URL_FEATURE_EXTRACTOR_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/macros.h"

class GURL;

namespace safe_browsing {
class FeatureMap;

class PhishingUrlFeatureExtractor {
 public:
  PhishingUrlFeatureExtractor();
  ~PhishingUrlFeatureExtractor();

  // Extracts features for |url| into the given feature map.
  // Returns true on success.
  bool ExtractFeatures(const GURL& url, FeatureMap* features);

 private:
  friend class PhishingUrlFeatureExtractorTest;

  static const size_t kMinPathComponentLength = 3;

  // Given a string, finds all substrings of consecutive alphanumeric
  // characters of length >= kMinPathComponentLength and inserts them into
  // tokens.
  static void SplitStringIntoLongAlphanumTokens(
      const std::string& full,
      std::vector<std::string>* tokens);

  DISALLOW_COPY_AND_ASSIGN(PhishingUrlFeatureExtractor);
};

}  // namespace safe_browsing

#endif  // CHROME_RENDERER_SAFE_BROWSING_PHISHING_URL_FEATURE_EXTRACTOR_H_
