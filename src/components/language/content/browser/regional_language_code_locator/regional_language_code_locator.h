// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_CONTENT_BROWSER_REGIONAL_LANGUAGE_CODE_LOCATOR_REGIONAL_LANGUAGE_CODE_LOCATOR_H_
#define COMPONENTS_LANGUAGE_CONTENT_BROWSER_REGIONAL_LANGUAGE_CODE_LOCATOR_REGIONAL_LANGUAGE_CODE_LOCATOR_H_

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "components/language/content/browser/language_code_locator.h"

namespace language {

class RegionalLanguageCodeLocator : public LanguageCodeLocator {
 public:
  RegionalLanguageCodeLocator();
  ~RegionalLanguageCodeLocator() override;

  // LanguageCodeLocator implementation.
  std::vector<std::string> GetLanguageCodes(double latitude,
                                            double longitude) const override;

 private:
  // Map from s2 cellid to ';' delimited list of language codes enum.
  base::flat_map<uint32_t, char> district_languages_;

  DISALLOW_COPY_AND_ASSIGN(RegionalLanguageCodeLocator);
};

}  // namespace language

#endif  // COMPONENTS_LANGUAGE_CONTENT_BROWSER_REGIONAL_LANGUAGE_CODE_LOCATOR_REGIONAL_LANGUAGE_CODE_LOCATOR_H_
