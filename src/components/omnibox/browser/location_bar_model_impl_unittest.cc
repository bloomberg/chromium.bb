// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/location_bar_model_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/omnibox/browser/location_bar_model_delegate.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

class FakeLocationBarModelDelegate : public LocationBarModelDelegate {
 public:
  void SetURL(const GURL& url) { url_ = url; }

  // LocationBarModelDelegate:
  base::string16 FormattedStringWithEquivalentMeaning(
      const GURL& url,
      const base::string16& formatted_url) const override {
    return formatted_url + base::ASCIIToUTF16("/TestSuffix");
  }

  bool GetURL(GURL* url) const override {
    *url = url_;
    return true;
  }

 private:
  GURL url_;
};

TEST(LocationBarModelImplTest,
     DisplayUrlAppliesFormattedStringWithEquivalentMeaning) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({omnibox::kHideSteadyStateUrlScheme,
                                 omnibox::kHideSteadyStateUrlTrivialSubdomains},
                                {});

  FakeLocationBarModelDelegate delegate;
  auto model = std::make_unique<LocationBarModelImpl>(&delegate, 1024);

  delegate.SetURL(GURL("http://www.google.com/"));

  // Verify that both the full formatted URL and the display URL add the test
  // suffix.
  EXPECT_EQ(base::ASCIIToUTF16("www.google.com/TestSuffix"),
            model->GetFormattedFullURL());
  EXPECT_EQ(base::ASCIIToUTF16("google.com/TestSuffix"),
            model->GetURLForDisplay());
}

}  // namespace
