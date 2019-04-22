// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_LOCAL_NTP_BROWSERTEST_BASE_H_
#define CHROME_BROWSER_UI_SEARCH_LOCAL_NTP_BROWSERTEST_BASE_H_

#include <string>
#include <vector>

#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_observer.h"
#include "chrome/common/search/instant_types.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/native_theme/test_native_theme.h"

// Test observer for InstantService notifications.
class TestInstantServiceObserver : public InstantServiceObserver {
 public:
  explicit TestInstantServiceObserver(InstantService* service);

  ~TestInstantServiceObserver() override;

  void WaitForMostVisitedItems(size_t count);

  void WaitForThemeInfoUpdated(std::string background_url,
                               std::string attribution_1,
                               std::string attribution_2,
                               std::string attribution_action_url);

  void WaitForThemeApplied(bool theme_installed);

  void WaitForDarkModeApplied(bool dark_mode);

  bool IsUsingDefaultTheme();

 private:
  void ThemeInfoChanged(const ThemeBackgroundInfo& theme_info) override;

  void MostVisitedItemsChanged(const std::vector<InstantMostVisitedItem>& items,
                               bool is_custom_links) override;

  InstantService* const service_;

  std::vector<InstantMostVisitedItem> items_;
  ThemeBackgroundInfo theme_info_;

  bool theme_installed_;

  size_t expected_count_;
  std::string expected_background_url_;
  std::string expected_attribution_1_;
  std::string expected_attribution_2_;
  std::string expected_attribution_action_url_;
  bool expected_dark_mode_ = false;

  base::OnceClosure quit_closure_most_visited_;
  base::OnceClosure quit_closure_custom_background_;
  base::OnceClosure quit_closure_theme_;
  base::OnceClosure quit_closure_dark_mode_;
};

// Base class for dark mode tests on the local NTP. Provides helper functions to
// determine if dark mode styling is properly applied.
class DarkModeTestBase {
 protected:
  DarkModeTestBase();

  ui::TestNativeTheme* theme() { return &theme_; }

  // Returns true if dark mode is applied on the |frame|.
  bool GetIsDarkModeApplied(const content::ToRenderFrameHost& frame);

  // Returns true if dark mode is applied on the |frame| but not on the chips
  // (i.e. Most Visited icons, etc). This occurs when a background image is set.
  bool GetIsLightChipsApplied(const content::ToRenderFrameHost& frame);

  // Returns true if dark mode is applied to the Most Visited icon at |index|
  // (i.e. the icon URL contains the |kMVIconDarkParameter|).
  bool GetIsDarkTile(const content::ToRenderFrameHost& frame, int index);

 private:
  ui::TestNativeTheme theme_;
};

#endif  // CHROME_BROWSER_UI_SEARCH_LOCAL_NTP_BROWSERTEST_BASE_H_
