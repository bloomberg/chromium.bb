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

#include "ash/app_list/model/search/search_model.h"
#include "ash/app_list/model/speech/speech_ui_model.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/app_list/app_list_view_delegate.h"
#include "ui/app_list/test/app_list_test_model.h"

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
  std::map<size_t, int>& open_search_result_counts() {
    return open_search_result_counts_;
  }

  // Sets the number of apps that the model will be created with the next time
  // SetProfileByPath() is called.
  void set_next_profile_app_count(int apps) { next_profile_app_count_ = apps; }

  // Returns the value of |stop_speech_recognition_count_| and then resets this
  // value to 0.
  int GetStopSpeechRecognitionCountAndReset();

  // Sets whether the search engine is Google or not.
  void SetSearchEngineIsGoogle(bool is_google);

  // AppListViewDelegate overrides:
  AppListModel* GetModel() override;
  SearchModel* GetSearchModel() override;
  SpeechUIModel* GetSpeechUI() override;
  void StartSearch(const base::string16& raw_query) override {}
  void OpenSearchResult(SearchResult* result,
                        int event_flags) override;
  void InvokeSearchResultAction(SearchResult* result,
                                int action_index,
                                int event_flags) override {}
  void ViewInitialized() override {}
  void Dismiss() override;
  void ViewClosing() override {}
  void StartSpeechRecognition() override {}
  void StopSpeechRecognition() override;
  views::View* CreateStartPageWebView(const gfx::Size& size) override;
  bool IsSpeechRecognitionEnabled() override;
  void GetWallpaperProminentColors(std::vector<SkColor>* colors) override {}
  void ActivateItem(const std::string& id, int event_flags) override;
  ui::MenuModel* GetContextMenuModel(const std::string& id) override;
  void AddObserver(app_list::AppListViewDelegateObserver* observer) override {}
  void RemoveObserver(
      app_list::AppListViewDelegateObserver* observer) override {}

  // Do a bulk replacement of the items in the model.
  void ReplaceTestModel(int item_count);

  AppListTestModel* ReleaseTestModel() { return model_.release(); }
  AppListTestModel* GetTestModel() { return model_.get(); }

 private:
  int dismiss_count_ = 0;
  int stop_speech_recognition_count_ = 0;
  int open_search_result_count_ = 0;
  int next_profile_app_count_ = 0;
  std::map<size_t, int> open_search_result_counts_;
  std::unique_ptr<AppListTestModel> model_;
  std::unique_ptr<SearchModel> search_model_;
  SpeechUIModel speech_ui_;
  std::vector<SkColor> wallpaper_prominent_colors_;

  DISALLOW_COPY_AND_ASSIGN(AppListTestViewDelegate);
};

}  // namespace test
}  // namespace app_list

#endif  // UI_APP_LIST_TEST_APP_LIST_TEST_VIEW_DELEGATE_H_
