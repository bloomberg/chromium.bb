// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_TEST_APP_LIST_TEST_VIEW_DELEGATE_H_
#define UI_APP_LIST_TEST_APP_LIST_TEST_VIEW_DELEGATE_H_

#include <map>
#include <string>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "ui/app_list/app_list_view_delegate.h"
#include "ui/app_list/speech_ui_model.h"

namespace app_list {
namespace test {

class AppListTestModel;

// A concrete AppListViewDelegate for unit tests.
class AppListTestViewDelegate : public AppListViewDelegate {
 public:
  AppListTestViewDelegate();
  virtual ~AppListTestViewDelegate();

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

  // Returns the value of |toggle_speech_recognition_count_| and then
  // resets this value to 0.
  int GetToggleSpeechRecognitionCountAndReset();

  // AppListViewDelegate overrides:
  virtual bool ForceNativeDesktop() const OVERRIDE;
  virtual void SetProfileByPath(const base::FilePath& profile_path) OVERRIDE;
  virtual AppListModel* GetModel() OVERRIDE;
  virtual SpeechUIModel* GetSpeechUI() OVERRIDE;
  virtual void GetShortcutPathForApp(
      const std::string& app_id,
      const base::Callback<void(const base::FilePath&)>& callback) OVERRIDE;
  virtual void StartSearch() OVERRIDE {}
  virtual void StopSearch() OVERRIDE {}
  virtual void OpenSearchResult(SearchResult* result,
                                bool auto_launch,
                                int event_flags) OVERRIDE;
  virtual void InvokeSearchResultAction(SearchResult* result,
                                        int action_index,
                                        int event_flags) OVERRIDE {}
  virtual base::TimeDelta GetAutoLaunchTimeout() OVERRIDE;
  virtual void AutoLaunchCanceled() OVERRIDE;
  virtual void ViewInitialized() OVERRIDE {}
  virtual void Dismiss() OVERRIDE;
  virtual void ViewClosing() OVERRIDE {}
  virtual gfx::ImageSkia GetWindowIcon() OVERRIDE;
  virtual void OpenSettings() OVERRIDE {}
  virtual void OpenHelp() OVERRIDE {}
  virtual void OpenFeedback() OVERRIDE {}
  virtual void ToggleSpeechRecognition() OVERRIDE;
  virtual void ShowForProfileByPath(
      const base::FilePath& profile_path) OVERRIDE {}
#if defined(TOOLKIT_VIEWS)
  virtual views::View* CreateStartPageWebView(const gfx::Size& size) OVERRIDE;
  virtual std::vector<views::View*> CreateCustomPageWebViews(
      const gfx::Size& size) OVERRIDE;
#endif
  virtual bool IsSpeechRecognitionEnabled() OVERRIDE;
  virtual const Users& GetUsers() const OVERRIDE;
  virtual bool ShouldCenterWindow() const OVERRIDE;
  virtual void AddObserver(AppListViewDelegateObserver* observer) OVERRIDE;
  virtual void RemoveObserver(AppListViewDelegateObserver* observer) OVERRIDE;

  // Do a bulk replacement of the items in the model.
  void ReplaceTestModel(int item_count);

  AppListTestModel* ReleaseTestModel() { return model_.release(); }
  AppListTestModel* GetTestModel() { return model_.get(); }

 private:
  int dismiss_count_;
  int toggle_speech_recognition_count_;
  int open_search_result_count_;
  int next_profile_app_count_;
  std::map<size_t, int> open_search_result_counts_;
  Users users_;
  scoped_ptr<AppListTestModel> model_;
  ObserverList<AppListViewDelegateObserver> observers_;
  SpeechUIModel speech_ui_;
  base::TimeDelta auto_launch_timeout_;

  DISALLOW_COPY_AND_ASSIGN(AppListTestViewDelegate);
};

}  // namespace test
}  // namespace app_list

#endif  // UI_APP_LIST_TEST_APP_LIST_TEST_VIEW_DELEGATE_H_
