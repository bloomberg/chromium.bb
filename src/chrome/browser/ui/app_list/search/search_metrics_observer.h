// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_METRICS_OBSERVER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_METRICS_OBSERVER_H_

#include <string>
#include <vector>

#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/public/cpp/app_list/app_list_notifier.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/strings/string16.h"

namespace app_list {

class SearchController;

// Records impression, abandonment, and launch UMA metrics reported by the
// AppListNotifier.
class SearchMetricsObserver : ash::AppListNotifier::Observer {
 public:
  SearchMetricsObserver(ash::AppListNotifier* notifier,
                        SearchController* controller);
  ~SearchMetricsObserver() override;

  SearchMetricsObserver(const SearchMetricsObserver&) = delete;
  SearchMetricsObserver& operator=(const SearchMetricsObserver&) = delete;

  // AppListNotifier::Observer:
  void OnImpression(ash::AppListNotifier::Location location,
                    const std::vector<std::string>& results,
                    const base::string16& query) override;
  void OnAbandon(ash::AppListNotifier::Location location,
                 const std::vector<std::string>& results,
                 const base::string16& query) override;
  void OnLaunch(ash::AppListNotifier::Location location,
                const std::string& launched,
                const std::vector<std::string>& shown,
                const base::string16& query) override;

 private:
  // Looks up the ChromeSearchResult object in SearchController that corresponds
  // to |result_id|, and returns its type. If the result is not found, returns
  // base::nullopt and logs an error to UMA.
  base::Optional<ash::SearchResultType> GetType(const std::string& result_id);

  SearchController* controller_;
  ScopedObserver<ash::AppListNotifier, ash::AppListNotifier::Observer>
      observer_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_METRICS_OBSERVER_H_
