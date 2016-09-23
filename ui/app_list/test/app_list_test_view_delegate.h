// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_TEST_APP_LIST_TEST_VIEW_DELEGATE_H_
#define UI_APP_LIST_TEST_APP_LIST_TEST_VIEW_DELEGATE_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/app_list/app_list_view_delegate.h"
#include "ui/app_list/speech_ui_model.h"

namespace app_list {
namespace test {

class AppListTestModel;

// A concrete AppListViewDelegate for unit tests.
class AppListTestViewDelegate : public AppListViewDelegate {
 public:
  AppListTestViewDelegate();
  ~AppListTestViewDelegate() override;

  int dismiss_count() { return dismiss_count_; }
  int open_search_result_count() { return open_search_result_count_; }
  void SetUsers(const Users& users) {
    users_ = users;
  }
  std::map<size_t, int> open_search_result_counts() {
    return open_search_result_counts_;
  }

  // Sets the number of apps that the model will be created with the next time
  // SetProfileByPath() is called.
  void set_next_profile_app_count(int apps) { next_profile_app_count_ = apps; }

  void set_auto_launch_timeout(const base::TimeDelta& timeout) {
    auto_launch_timeout_ = timeout;
  }

  // Returns the value of |stop_speech_recognition_count_| and then resets this
  // value to 0.
  int GetStopSpeechRecognitionCountAndReset();

  // AppListViewDelegate overrides:
  bool ForceNativeDesktop() const override;
  void SetProfileByPath(const base::FilePath& profile_path) override;
  AppListModel* GetModel() override;
  SpeechUIModel* GetSpeechUI() override;
  void StartSearch() override {}
  void StopSearch() override {}
  void OpenSearchResult(SearchResult* result,
                        bool auto_launch,
                        int event_flags) override;
  void InvokeSearchResultAction(SearchResult* result,
                                int action_index,
                                int event_flags) override {}
  base::TimeDelta GetAutoLaunchTimeout() override;
  void AutoLaunchCanceled() override;
  void ViewInitialized() override {}
  void Dismiss() override;
  void ViewClosing() override {}
  void OpenHelp() override {}
  void OpenFeedback() override {}
  void StartSpeechRecognition() override {}
  void StopSpeechRecognition() override;
  void ShowForProfileByPath(const base::FilePath& profile_path) override {}
#if defined(TOOLKIT_VIEWS)
  views::View* CreateStartPageWebView(const gfx::Size& size) override;
  std::vector<views::View*> CreateCustomPageWebViews(
      const gfx::Size& size) override;
  void CustomLauncherPageAnimationChanged(double progress) override {}
  void CustomLauncherPagePopSubpage() override {}
#endif
  bool IsSpeechRecognitionEnabled() override;
  const Users& GetUsers() const override;

  // Do a bulk replacement of the items in the model.
  void ReplaceTestModel(int item_count);

  AppListTestModel* ReleaseTestModel() { return model_.release(); }
  AppListTestModel* GetTestModel() { return model_.get(); }

 private:
  int dismiss_count_;
  int stop_speech_recognition_count_;
  int open_search_result_count_;
  int next_profile_app_count_;
  std::map<size_t, int> open_search_result_counts_;
  Users users_;
  std::unique_ptr<AppListTestModel> model_;
  SpeechUIModel speech_ui_;
  base::TimeDelta auto_launch_timeout_;

  DISALLOW_COPY_AND_ASSIGN(AppListTestViewDelegate);
};

}  // namespace test
}  // namespace app_list

#endif  // UI_APP_LIST_TEST_APP_LIST_TEST_VIEW_DELEGATE_H_
