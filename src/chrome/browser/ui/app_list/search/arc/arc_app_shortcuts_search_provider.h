// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_ARC_ARC_APP_SHORTCUTS_SEARCH_PROVIDER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_ARC_ARC_APP_SHORTCUTS_SEARCH_PROVIDER_H_

#include <string>
#include <vector>

#include "ash/components/arc/mojom/app.mojom-forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/app_list/search/search_provider.h"

class AppListControllerDelegate;
class Profile;

namespace app_list {

class ArcAppShortcutsSearchProvider : public SearchProvider {
 public:
  ArcAppShortcutsSearchProvider(int max_results,
                                Profile* profile,
                                AppListControllerDelegate* list_controller);

  ArcAppShortcutsSearchProvider(const ArcAppShortcutsSearchProvider&) = delete;
  ArcAppShortcutsSearchProvider& operator=(
      const ArcAppShortcutsSearchProvider&) = delete;

  ~ArcAppShortcutsSearchProvider() override;

  // SearchProvider:
  void Start(const std::u16string& query) override;
  ash::AppListSearchResultType ResultType() const override;

 private:
  void OnGetAppShortcutGlobalQueryItems(
      std::vector<arc::mojom::AppShortcutItemPtr> shortcut_items);
  void UpdateRecommendedResults(
      std::vector<arc::mojom::AppShortcutItemPtr> shortcut_items);

  std::u16string last_query_;
  const int max_results_;
  Profile* const profile_;                            // Owned by ProfileInfo.
  AppListControllerDelegate* const list_controller_;  // Owned by AppListClient.

  base::WeakPtrFactory<ArcAppShortcutsSearchProvider> weak_ptr_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_ARC_ARC_APP_SHORTCUTS_SEARCH_PROVIDER_H_
