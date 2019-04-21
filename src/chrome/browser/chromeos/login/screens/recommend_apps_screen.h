// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_RECOMMEND_APPS_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_RECOMMEND_APPS_SCREEN_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"
#include "chrome/browser/chromeos/login/screens/recommend_apps/recommend_apps_fetcher_delegate.h"
#include "chrome/browser/chromeos/login/screens/recommend_apps_screen_view.h"

namespace base {
class Value;
}

namespace chromeos {

class RecommendAppsFetcher;

// This is Recommend Apps screen that is displayed as a part of user first
// sign-in flow.
class RecommendAppsScreen : public BaseScreen,
                            public RecommendAppsScreenViewObserver,
                            public RecommendAppsFetcherDelegate {
 public:
  enum class Result { SELECTED, SKIPPED };

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;
  RecommendAppsScreen(RecommendAppsScreenView* view,
                      const ScreenExitCallback& exit_callback);
  ~RecommendAppsScreen() override;

  // BaseScreen:
  void Show() override;
  void Hide() override;

  // RecommendAppsScreenViewObserver:
  void OnSkip() override;
  void OnRetry() override;
  void OnInstall() override;
  void OnViewDestroyed(RecommendAppsScreenView* view) override;

  // RecommendAppsFetcherDelegate:
  void OnLoadSuccess(const base::Value& app_list) override;
  void OnLoadError() override;
  void OnParseResponseError() override;

 private:
  RecommendAppsScreenView* view_;
  ScreenExitCallback exit_callback_;

  std::unique_ptr<RecommendAppsFetcher> recommend_apps_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(RecommendAppsScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_RECOMMEND_APPS_SCREEN_H_
