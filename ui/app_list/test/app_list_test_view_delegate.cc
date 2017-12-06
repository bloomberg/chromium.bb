// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/test/app_list_test_view_delegate.h"

#include <string>
#include <vector>

#include "ash/app_list/model/app_list_model.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/gfx/image/image_skia.h"

namespace app_list {
namespace test {

AppListTestViewDelegate::AppListTestViewDelegate()
    : model_(std::make_unique<AppListTestModel>()),
      search_model_(std::make_unique<SearchModel>()) {
}

AppListTestViewDelegate::~AppListTestViewDelegate() {}

int AppListTestViewDelegate::GetStopSpeechRecognitionCountAndReset() {
  int count = stop_speech_recognition_count_;
  stop_speech_recognition_count_ = 0;
  return count;
}

AppListModel* AppListTestViewDelegate::GetModel() {
  return model_.get();
}

SearchModel* AppListTestViewDelegate::GetSearchModel() {
  return search_model_.get();
}

SpeechUIModel* AppListTestViewDelegate::GetSpeechUI() {
  return &speech_ui_;
}

void AppListTestViewDelegate::OpenSearchResult(SearchResult* result,
                                               bool auto_launch,
                                               int event_flags) {
  const SearchModel::SearchResults* results = search_model_->results();
  for (size_t i = 0; i < results->item_count(); ++i) {
    if (results->GetItemAt(i) == result) {
      open_search_result_counts_[i]++;
      break;
    }
  }
  ++open_search_result_count_;
}

base::TimeDelta AppListTestViewDelegate::GetAutoLaunchTimeout() {
  return auto_launch_timeout_;
}

void AppListTestViewDelegate::AutoLaunchCanceled() {
  auto_launch_timeout_ = base::TimeDelta();
}

void AppListTestViewDelegate::Dismiss() {
  ++dismiss_count_;
}

void AppListTestViewDelegate::StopSpeechRecognition() {
  ++stop_speech_recognition_count_;
}

views::View* AppListTestViewDelegate::CreateStartPageWebView(
    const gfx::Size& size) {
  return NULL;
}
std::vector<views::View*> AppListTestViewDelegate::CreateCustomPageWebViews(
    const gfx::Size& size) {
  return std::vector<views::View*>();
}

bool AppListTestViewDelegate::IsSpeechRecognitionEnabled() {
  return false;
}

void AppListTestViewDelegate::ReplaceTestModel(int item_count) {
  model_ = std::make_unique<AppListTestModel>();
  model_->PopulateApps(item_count);
  search_model_ = std::make_unique<SearchModel>();
}

void AppListTestViewDelegate::SetSearchEngineIsGoogle(bool is_google) {
  search_model_->SetSearchEngineIsGoogle(is_google);
}

}  // namespace test
}  // namespace app_list
