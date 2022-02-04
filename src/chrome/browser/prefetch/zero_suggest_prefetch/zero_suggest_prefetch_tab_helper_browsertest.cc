// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/prefetch/zero_suggest_prefetch/zero_suggest_prefetch_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/mock_autocomplete_provider_client.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_client.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

class MockAutocompleteController : public AutocompleteController {
 public:
  MockAutocompleteController(
      std::unique_ptr<AutocompleteProviderClient> provider_client,
      int provider_types)
      : AutocompleteController(std::move(provider_client), provider_types) {}
  ~MockAutocompleteController() override = default;
  MockAutocompleteController(const MockAutocompleteController&) = delete;
  MockAutocompleteController& operator=(const MockAutocompleteController&) =
      delete;

  // AutocompleteController:
  MOCK_METHOD1(Start, void(const AutocompleteInput&));
  MOCK_METHOD1(StartPrefetch, void(const AutocompleteInput&));
};

}  // namespace

class ZeroSuggestPrefetchTabHelperBrowserTest : public InProcessBrowserTest {
 public:
  ZeroSuggestPrefetchTabHelperBrowserTest() {
    feature_list_.InitWithFeatures({omnibox::kZeroSuggestPrefetching}, {});
  }

 protected:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    auto template_url_service = std::make_unique<TemplateURLService>(
        /*prefs=*/nullptr, std::make_unique<SearchTermsData>(),
        /*web_data_service=*/nullptr,
        std::unique_ptr<TemplateURLServiceClient>(), base::RepeatingClosure());

    auto client_ = std::make_unique<MockAutocompleteProviderClient>();
    client_->set_template_url_service(std::move(template_url_service));

    auto controller =
        std::make_unique<testing::NiceMock<MockAutocompleteController>>(
            std::move(client_), 0);
    controller_ = controller.get();
    browser()
        ->window()
        ->GetLocationBar()
        ->GetOmniboxView()
        ->model()
        ->set_autocomplete_controller(std::move(controller));
  }

  base::test::ScopedFeatureList feature_list_;
  raw_ptr<testing::NiceMock<MockAutocompleteController>> controller_;
};

// Tests that navigating to or switching to the NTP starts a prefetch request.
IN_PROC_BROWSER_TEST_F(ZeroSuggestPrefetchTabHelperBrowserTest, StartPrefetch) {
  {
    // Opening a background NTP does not trigger prefetching.
    EXPECT_CALL(*controller_, StartPrefetch).Times(0);
    EXPECT_CALL(*controller_, Start).Times(0);

    EXPECT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL(chrome::kChromeUINewTabURL),
        WindowOpenDisposition::NEW_BACKGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
    ASSERT_EQ(2, browser()->tab_strip_model()->GetTabCount());

    testing::Mock::VerifyAndClearExpectations(controller_);
  }
  {
    // Navigating to a url in a new foreground tab does not trigger prefetching.
    EXPECT_CALL(*controller_, StartPrefetch).Times(0);
    EXPECT_CALL(*controller_, Start).Times(0);

    ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL("https://foo.com"),
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_NONE);
    ASSERT_EQ(3, browser()->tab_strip_model()->GetTabCount());

    testing::Mock::VerifyAndClearExpectations(controller_);
  }
  {
    // Navigating to the NTP in the current foreground tab does not trigger
    // prefetching. This is a rare case we do not need to cover for simplicity.
    EXPECT_CALL(*controller_, StartPrefetch).Times(0);
    EXPECT_CALL(*controller_, Start).Times(0);

    EXPECT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL(chrome::kChromeUINewTabURL),
        WindowOpenDisposition::CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
    ASSERT_EQ(3, browser()->tab_strip_model()->GetTabCount());

    testing::Mock::VerifyAndClearExpectations(controller_);
  }
  {
    // Opening a foreground NTP triggers prefetching.
    EXPECT_CALL(*controller_, StartPrefetch).Times(1);
    EXPECT_CALL(*controller_, Start).Times(0);

    EXPECT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL(chrome::kChromeUINewTabURL),
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
    ASSERT_EQ(4, browser()->tab_strip_model()->GetTabCount());

    testing::Mock::VerifyAndClearExpectations(controller_);
  }
  {
    // Switching to an NTP triggers prefetching.
    EXPECT_CALL(*controller_, StartPrefetch).Times(1);
    EXPECT_CALL(*controller_, Start).Times(0);

    browser()->tab_strip_model()->ActivateTabAt(1);

    testing::Mock::VerifyAndClearExpectations(controller_);
  }
}
