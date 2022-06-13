// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_metrics_observer.h"

#include "base/containers/flat_set.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/search_controller.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace app_list {
namespace {

using Result = ash::AppListNotifier::Result;
using Location = ash::AppListNotifier::Location;

// Represents possible error states of the metrics observer itself. These values
// persist to logs. Entries should not be renumbered and numeric values should
// never be reused.
enum class Error {
  kMissingNotifier = 0,
  kResultNotFound = 1,
  kUntrackedLocation = 2,
  kUntypedResult = 3,
  kMaxValue = kUntypedResult
};

// Represents the actions a user can take in the launcher. These values persist
// to logs. Entries should not be renumbered and numeric values should never be
// reused.
enum class Action {
  kImpression = 0,
  kLaunch = 1,
  kAbandon = 2,
  kIgnore = 3,
  kMaxValue = kIgnore
};

char kHistogramPrefix[] = "Apps.AppList.Search.";

void LogError(Error error) {
  base::UmaHistogramEnumeration(base::StrCat({kHistogramPrefix, "Error"}),
                                error);
}

std::string GetViewString(Location location, const std::u16string& raw_query) {
  std::u16string query;
  base::TrimWhitespace(raw_query, base::TRIM_ALL, &query);
  switch (location) {
    case Location::kList:
      return query.empty() ? "ListZeroState" : "ListSearch";
    case Location::kTile:
      return query.empty() ? "AppsZeroState" : "AppsSearch";
    case Location::kChip:
      return "Chips";
    default:
      LogError(Error::kUntrackedLocation);
      return "Untracked";
  }
}

std::set<ash::SearchResultType> TypeSet(const std::vector<Result>& results) {
  std::set<ash::SearchResultType> types;
  for (const auto& result : results) {
    types.insert(result.type);
  }
  return types;
}

// Log an action on a set of search result types.
void LogTypeActions(const std::string& action_name,
                    Location location,
                    const std::u16string& query,
                    const std::set<ash::SearchResultType>& types) {
  const std::string histogram_name = base::StrCat(
      {kHistogramPrefix, GetViewString(location, query), ".", action_name});

  for (auto type : types) {
    if (type == ash::SEARCH_RESULT_TYPE_BOUNDARY) {
      LogError(Error::kUntypedResult);
    } else {
      base::UmaHistogramEnumeration(histogram_name, type,
                                    ash::SEARCH_RESULT_TYPE_BOUNDARY);
    }
  }
}

// Log an action for a search result view as a whole.
void LogViewAction(Location location,
                   const std::u16string& query,
                   Action action) {
  const std::string histogram_name =
      base::StrCat({kHistogramPrefix, GetViewString(location, query)});
  base::UmaHistogramEnumeration(histogram_name, action);
}

}  // namespace

SearchMetricsObserver::SearchMetricsObserver(ash::AppListNotifier* notifier) {
  if (notifier) {
    observation_.Observe(notifier);
  } else {
    LogError(Error::kMissingNotifier);
  }
}

SearchMetricsObserver::~SearchMetricsObserver() = default;

void SearchMetricsObserver::OnImpression(Location location,
                                         const std::vector<Result>& results,
                                         const std::u16string& query) {
  LogTypeActions("Impression", location, query, TypeSet(results));
  LogViewAction(location, query, Action::kImpression);
}

void SearchMetricsObserver::OnAbandon(Location location,
                                      const std::vector<Result>& results,
                                      const std::u16string& query) {
  LogTypeActions("Abandon", location, query, TypeSet(results));
  LogViewAction(location, query, Action::kAbandon);
}

void SearchMetricsObserver::OnLaunch(Location location,
                                     const Result& launched,
                                     const std::vector<Result>& shown,
                                     const std::u16string& query) {
  LogViewAction(location, query, Action::kLaunch);

  // Record an ignore for all result types in this view. If other views are
  // shown, they are handled by OnIgnore.
  std::set<ash::SearchResultType> types;
  for (const auto& result : shown) {
    if (result.type != launched.type) {
      types.insert(result.type);
    }
  }
  LogTypeActions("Ignore", location, query, types);
  LogTypeActions("Launch", location, query, {launched.type});

  // Record the launch index.
  int launched_index = -1;
  for (int i = 0; i < shown.size(); ++i) {
    if (shown[i].id == launched.id) {
      launched_index = i;
      break;
    }
  }
  const std::string histogram_name = base::StrCat(
      {kHistogramPrefix, GetViewString(location, query), ".Index"});
  base::UmaHistogramExactLinear(histogram_name, launched_index, 50);
}

void SearchMetricsObserver::OnIgnore(Location location,
                                     const std::vector<Result>& results,
                                     const std::u16string& query) {
  // We have no two concurrently displayed views showing the same result types,
  // so it's safe to log an ignore for all result types here.
  LogTypeActions("Ignore", location, query, TypeSet(results));
  LogViewAction(location, query, Action::kIgnore);
}

}  // namespace app_list
