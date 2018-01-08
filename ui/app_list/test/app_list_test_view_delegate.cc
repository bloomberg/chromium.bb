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

void AppListTestViewDelegate::ActivateItem(const std::string& id,
                                           int event_flags) {
  app_list::AppListItem* item = model_->FindItem(id);
  if (!item)
    return;
  DCHECK(!item->is_folder());
  static_cast<AppListTestModel::AppListTestItem*>(item)->Activate(event_flags);
}

ui::MenuModel* AppListTestViewDelegate::GetContextMenuModel(
    const std::string& id) {
  app_list::AppListItem* item = model_->FindItem(id);
  // TODO(stevenjb/jennyz): Implement this for folder items
  if (!item || item->is_folder())
    return nullptr;
  return static_cast<AppListTestModel::AppListTestItem*>(item)
      ->GetContextMenuModel();
}

}  // namespace test
}  // namespace app_list
