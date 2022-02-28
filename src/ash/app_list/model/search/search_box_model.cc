// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/model/search/search_box_model.h"

#include <utility>

#include "ash/app_list/model/search/search_box_model_observer.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"

namespace ash {

SearchBoxModel::SearchBoxModel() = default;

SearchBoxModel::~SearchBoxModel() = default;

void SearchBoxModel::SetShowAssistantButton(bool show) {
  if (show_assistant_button_ == show)
    return;
  show_assistant_button_ = show;
  for (auto& observer : observers_)
    observer.ShowAssistantChanged();
}

void SearchBoxModel::SetSearchEngineIsGoogle(bool is_google) {
  if (is_google == search_engine_is_google_)
    return;
  search_engine_is_google_ = is_google;
  for (auto& observer : observers_)
    observer.SearchEngineChanged();
}

void SearchBoxModel::Update(const std::u16string& text,
                            bool initiated_by_user) {
  if (text_ == text)
    return;

  if (initiated_by_user) {
    if (text_.empty() && !text.empty()) {
      UMA_HISTOGRAM_ENUMERATION("Apps.AppListSearchCommenced", 1, 2);
      base::RecordAction(base::UserMetricsAction("AppList_EnterSearch"));
    } else if (!text_.empty() && text.empty()) {
      base::RecordAction(base::UserMetricsAction("AppList_LeaveSearch"));
    }
  }
  text_ = text;
  for (auto& observer : observers_)
    observer.Update();
}

void SearchBoxModel::AddObserver(SearchBoxModelObserver* observer) {
  observers_.AddObserver(observer);
}

void SearchBoxModel::RemoveObserver(SearchBoxModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace ash
