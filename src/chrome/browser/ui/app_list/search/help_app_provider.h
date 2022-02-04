// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_HELP_APP_PROVIDER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_HELP_APP_PROVIDER_H_

#include <string>
#include <vector>

#include "ash/webui/help_app_ui/search/search.mojom.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/search_provider.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;

namespace ash {
namespace help_app {
class SearchHandler;
}  // namespace help_app
}  // namespace ash

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace app_list {

// Search results for the Help App (aka Explore).
class HelpAppResult : public ChromeSearchResult {
 public:
  // Constructor for the What's new chip.
  HelpAppResult(Profile* profile,
                const std::string& id,
                const std::u16string& title,
                const gfx::ImageSkia& icon);
  // Constructor for a list result.
  HelpAppResult(const float& relevance,
                Profile* profile,
                const ash::help_app::mojom::SearchResultPtr& result,
                const gfx::ImageSkia& icon,
                const std::u16string& query);

  ~HelpAppResult() override;

  HelpAppResult(const HelpAppResult&) = delete;
  HelpAppResult& operator=(const HelpAppResult&) = delete;

  // ChromeSearchResult overrides:
  void Open(int event_flags) override;

 private:
  Profile* const profile_;
  const std::string url_path_;
  const std::string help_app_content_id_;
};

// Provides results from the Help App based on the search query. Also provides
// zero-state results.
class HelpAppProvider : public SearchProvider,
                        public apps::AppRegistryCache::Observer,
                        public ash::help_app::mojom::SearchResultsObserver {
 public:
  explicit HelpAppProvider(Profile* profile);
  ~HelpAppProvider() override;

  HelpAppProvider(const HelpAppProvider&) = delete;
  HelpAppProvider& operator=(const HelpAppProvider&) = delete;

  // SearchProvider:
  void Start(const std::u16string& query) override;
  void StartZeroState() override;
  void ViewClosing() override;
  void AppListShown() override;
  ash::AppListSearchResultType ResultType() const override;
  bool ShouldBlockZeroState() const override;

  // apps::AppRegistryCache::Observer:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

  // mojom::SearchResultsObserver:
  void OnSearchResultAvailabilityChanged() override;

 private:
  void OnSearchReturned(
      const std::u16string& query,
      const base::TimeTicks& start_time,
      std::vector<ash::help_app::mojom::SearchResultPtr> results);
  void OnLoadIcon(apps::IconValuePtr icon_value);
  void LoadIcon();

  Profile* const profile_;
  ash::help_app::SearchHandler* search_handler_;
  apps::AppServiceProxy* app_service_proxy_;
  gfx::ImageSkia icon_;

  // Last search query. It is reset when the view is closed.
  std::u16string last_query_;
  mojo::Receiver<ash::help_app::mojom::SearchResultsObserver>
      search_results_observer_receiver_{this};

  base::WeakPtrFactory<HelpAppProvider> weak_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_HELP_APP_PROVIDER_H_
