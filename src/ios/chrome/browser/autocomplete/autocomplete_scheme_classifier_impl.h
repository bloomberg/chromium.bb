// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_SCHEME_CLASSIFIER_IMPL_H_
#define IOS_CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_SCHEME_CLASSIFIER_IMPL_H_

#include "base/macros.h"
#include "components/omnibox/browser/autocomplete_scheme_classifier.h"

// AutocompleteSchemeClassifierImpl provides iOS-specific implementation of
// AutocompleteSchemeClassifier interface.
class AutocompleteSchemeClassifierImpl : public AutocompleteSchemeClassifier {
 public:
  AutocompleteSchemeClassifierImpl();
  ~AutocompleteSchemeClassifierImpl() override;

  // AutocompleteInputSchemeChecker implementation.
  metrics::OmniboxInputType GetInputTypeForScheme(
      const std::string& scheme) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(AutocompleteSchemeClassifierImpl);
};

#endif  // IOS_CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_SCHEME_CLASSIFIER_IMPL_H_
