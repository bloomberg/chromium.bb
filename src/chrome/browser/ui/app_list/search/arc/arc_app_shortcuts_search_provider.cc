// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/arc/arc_app_shortcuts_search_provider.h"

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/app_list/search/arc/arc_app_shortcut_search_result.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service_manager.h"

namespace app_list {

ArcAppShortcutsSearchProvider::ArcAppShortcutsSearchProvider(
    int max_results,
    Profile* profile,
    AppListControllerDelegate* list_controller)
    : max_results_(max_results),
      profile_(profile),
      list_controller_(list_controller),
      weak_ptr_factory_(this) {}

ArcAppShortcutsSearchProvider::~ArcAppShortcutsSearchProvider() = default;

void ArcAppShortcutsSearchProvider::Start(const base::string16& query) {
  arc::mojom::AppInstance* app_instance =
      arc::ArcServiceManager::Get()
          ? ARC_GET_INSTANCE_FOR_METHOD(
                arc::ArcServiceManager::Get()->arc_bridge_service()->app(),
                GetAppShortcutGlobalQueryItems)
          : nullptr;

  if (!app_instance || query.empty()) {
    ClearResults();
    return;
  }

  // Invalidate the weak ptr to prevent previous callback run.
  weak_ptr_factory_.InvalidateWeakPtrs();
  app_instance->GetAppShortcutGlobalQueryItems(
      base::UTF16ToUTF8(query), max_results_,
      base::BindOnce(
          &ArcAppShortcutsSearchProvider::OnGetAppShortcutGlobalQueryItems,
          weak_ptr_factory_.GetWeakPtr()));
}

void ArcAppShortcutsSearchProvider::OnGetAppShortcutGlobalQueryItems(
    std::vector<arc::mojom::AppShortcutItemPtr> shortcut_items) {
  SearchProvider::Results search_results;
  for (auto& item : shortcut_items) {
    search_results.emplace_back(std::make_unique<ArcAppShortcutSearchResult>(
        std::move(item), profile_, list_controller_));
  }
  SwapResults(&search_results);
}

}  // namespace app_list
