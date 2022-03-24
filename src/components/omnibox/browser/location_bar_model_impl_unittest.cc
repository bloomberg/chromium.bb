// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/location_bar_model_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/dom_distiller/core/url_utils.h"
#include "components/omnibox/browser/location_bar_model_delegate.h"
#include "components/omnibox/browser/test_omnibox_client.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/security_state/core/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/paint_vector_icon.h"
#include "url/gurl.h"
#if !defined(OS_ANDROID) && !defined(OS_IOS)
#include "components/omnibox/browser/vector_icons.h"  // nogncheck
#include "components/vector_icons/vector_icons.h"     // nogncheck
#endif

namespace {

class FakeLocationBarModelDelegate : public LocationBarModelDelegate {
 public:
  void SetURL(const GURL& url) { url_ = url; }
  void SetShouldPreventElision(bool should_prevent_elision) {
    should_prevent_elision_ = should_prevent_elision;
  }
  void SetSecurityLevel(security_state::SecurityLevel level) {
    security_level_ = level;
  }
  void SetVisibleSecurityStateConnectionInfoUninitialized() {
    connection_info_initialized_ = false;
  }

  // LocationBarModelDelegate:
  std::u16string FormattedStringWithEquivalentMeaning(
      const GURL& url,
      const std::u16string& formatted_url) const override {
    return formatted_url + u"/TestSuffix";
  }

  bool GetURL(GURL* url) const override {
    *url = url_;
    return true;
  }

  bool ShouldPreventElision() override { return should_prevent_elision_; }

  security_state::SecurityLevel GetSecurityLevel() const override {
    return security_level_;
  }

  std::unique_ptr<security_state::VisibleSecurityState>
  GetVisibleSecurityState() const override {
    std::unique_ptr<security_state::VisibleSecurityState> state =
        std::make_unique<security_state::VisibleSecurityState>();
    state->connection_info_initialized = connection_info_initialized_;
    return state;
  }

  bool IsNewTabPage() const override { return false; }

  bool IsNewTabPageURL(const GURL& url) const override { return false; }

  bool IsHomePage(const GURL& url) const override { return false; }

  AutocompleteClassifier* GetAutocompleteClassifier() override {
    return omnibox_client_.GetAutocompleteClassifier();
  }

  TemplateURLService* GetTemplateURLService() override {
    return omnibox_client_.GetTemplateURLService();
  }

 private:
  GURL url_;
  security_state::SecurityLevel security_level_;
  TestOmniboxClient omnibox_client_;
  bool should_prevent_elision_ = false;
  bool connection_info_initialized_ = true;
};

class LocationBarModelImplTest : public testing::Test {
 protected:
  const GURL kValidSearchResultsPage =
      GURL("https://www.google.com/search?q=foo+query");

  LocationBarModelImplTest() : model_(&delegate_, 1024) {}

  FakeLocationBarModelDelegate* delegate() { return &delegate_; }

  LocationBarModelImpl* model() { return &model_; }

 private:
  base::test::TaskEnvironment task_environment_;
  FakeLocationBarModelDelegate delegate_;
  LocationBarModelImpl model_;
};

TEST_F(LocationBarModelImplTest, FormatsReaderModeUrls) {
  const GURL http_url("http://www.example.com/article.html");
  // Get the real article's URL shown to the user.
  delegate()->SetURL(http_url);
  std::u16string originalDisplayUrl = model()->GetURLForDisplay();
  std::u16string originalFormattedFullUrl = model()->GetFormattedFullURL();
  // We expect that they don't start with "http://." We want the reader mode
  // URL shown to the user to be the same as this original URL.
#if defined(OS_IOS)
  EXPECT_EQ(u"example.com/TestSuffix", originalDisplayUrl);
#else   // #!defined(OS_IOS)
  EXPECT_EQ(u"example.com/article.html/TestSuffix", originalDisplayUrl);
#endif  // #defined (OS_IOS)
  EXPECT_EQ(u"www.example.com/article.html/TestSuffix",
            originalFormattedFullUrl);

  GURL distilled = dom_distiller::url_utils::GetDistillerViewUrlFromUrl(
      dom_distiller::kDomDistillerScheme, http_url, "title");
  // Ensure the test is set up properly by checking the reader mode URL has
  // the reader mode scheme.
  EXPECT_EQ(dom_distiller::kDomDistillerScheme, distilled.scheme());
  delegate()->SetURL(distilled);

  // The user should see the same URL seen for the original article.
  EXPECT_EQ(originalDisplayUrl, model()->GetURLForDisplay());
  EXPECT_EQ(originalFormattedFullUrl, model()->GetFormattedFullURL());

  // Similarly, https scheme should also be hidden, except from
  // GetFormattedFullURL, because kFormatUrlOmitDefaults does not omit https.
  const GURL https_url("https://www.example.com/article.html");
  distilled = dom_distiller::url_utils::GetDistillerViewUrlFromUrl(
      dom_distiller::kDomDistillerScheme, https_url, "title");
  delegate()->SetURL(distilled);
  EXPECT_EQ(originalDisplayUrl, model()->GetURLForDisplay());
  EXPECT_EQ(u"https://www.example.com/article.html/TestSuffix",
            model()->GetFormattedFullURL());

  // Invalid dom-distiller:// URLs should be shown, because they do not
  // correspond to any article.
  delegate()->SetURL(GURL(("chrome-distiller://abc/?url=invalid")));
#if defined(OS_IOS)
  EXPECT_EQ(u"chrome-distiller://abc/TestSuffix", model()->GetURLForDisplay());
#else   // #!defined(OS_IOS)
  EXPECT_EQ(u"chrome-distiller://abc/?url=invalid/TestSuffix",
            model()->GetURLForDisplay());
#endif  // #defined (OS_IOS)
  EXPECT_EQ(u"chrome-distiller://abc/?url=invalid/TestSuffix",
            model()->GetFormattedFullURL());
}

// TODO(https://crbug.com/1010418): Fix flakes on linux_chromium_asan_rel_ng and
// re-enable this test.
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#define MAYBE_PreventElisionWorks DISABLED_PreventElisionWorks
#else
#define MAYBE_PreventElisionWorks PreventElisionWorks
#endif
TEST_F(LocationBarModelImplTest, MAYBE_PreventElisionWorks) {
  delegate()->SetShouldPreventElision(true);
  delegate()->SetURL(GURL("https://www.google.com/search?q=foo+query+unelide"));

  EXPECT_EQ(u"https://www.google.com/search?q=foo+query+unelide/TestSuffix",
            model()->GetURLForDisplay());

  // Test that HTTP elisions are prevented.
  delegate()->SetURL(GURL("http://www.google.com/search?q=foo+query+unelide"));
  EXPECT_EQ(u"http://www.google.com/search?q=foo+query+unelide/TestSuffix",
            model()->GetURLForDisplay());
}

#if !defined(OS_ANDROID) && !defined(OS_IOS)
// Tests GetVectorIcon returns the correct security indicator icon.
TEST_F(LocationBarModelImplTest, GetVectorIcon) {
  delegate()->SetSecurityLevel(security_state::SecurityLevel::WARNING);

  gfx::ImageSkia expected_icon =
      gfx::CreateVectorIcon(vector_icons::kNotSecureWarningIcon,
                            gfx::kFaviconSize, gfx::kPlaceholderColor);

  gfx::ImageSkia icon = gfx::CreateVectorIcon(
      model()->GetVectorIcon(), gfx::kFaviconSize, gfx::kPlaceholderColor);

  EXPECT_EQ(icon.bitmap(), expected_icon.bitmap());
}
#endif  // !defined(OS_IOS)

#if defined(OS_IOS)

// Test that blob:http://example.test/foobar is displayed as "example.test" on
// iOS.
TEST_F(LocationBarModelImplTest, BlobDisplayURLIOS) {
  delegate()->SetURL(GURL("blob:http://example.test/foo"));
  EXPECT_EQ(u"example.test/TestSuffix", model()->GetURLForDisplay());
}

#endif  // defined(OS_IOS)

}  // namespace
