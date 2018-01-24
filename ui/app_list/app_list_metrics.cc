// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/app_list_metrics.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/model/search/search_model.h"
#include "ash/app_list/model/search/search_result.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"

namespace app_list {

// The UMA histogram that logs which state search results are opened from.
constexpr char kAppListSearchResultOpenSourceHistogram[] =
    "Apps.AppListSearchResultOpenedSource";

// The different sources from which a search result is displayed. These values
// are written to logs.  New enum values can be added, but existing enums must
// never be renumbered or deleted and reused.
enum class ApplistSearchResultOpenedSource {
  kHalfClamshell = 0,
  kFullscreenClamshell = 1,
  kFullscreenTablet = 2,
  kMaxApplistSearchResultOpenedSource = 3,
};

APP_LIST_EXPORT void RecordSearchResultOpenSource(
    const SearchResult* result,
    const AppListModel* model,
    const SearchModel* search_model) {
  // Record the search metric if the SearchResult is not a suggested app.
  if (result->display_type() == SearchResult::DISPLAY_RECOMMENDATION)
    return;

  ApplistSearchResultOpenedSource source;
  AppListViewState state = model->state_fullscreen();
  if (search_model->tablet_mode()) {
    source = ApplistSearchResultOpenedSource::kFullscreenTablet;
  } else {
    source = state == AppListViewState::HALF
                 ? ApplistSearchResultOpenedSource::kHalfClamshell
                 : ApplistSearchResultOpenedSource::kFullscreenClamshell;
  }
  UMA_HISTOGRAM_ENUMERATION(
      kAppListSearchResultOpenSourceHistogram, source,
      ApplistSearchResultOpenedSource::kMaxApplistSearchResultOpenedSource);
}

}  // namespace app_list
