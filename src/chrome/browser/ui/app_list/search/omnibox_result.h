// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_OMNIBOX_RESULT_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_OMNIBOX_RESULT_H_

#include <memory>

#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_delegate.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "url/gurl.h"

class AppListControllerDelegate;
class AutocompleteController;
class FaviconCache;
class BitmapFetcher;
class Profile;

// These are used in histograms, do not remove/renumber entries. If you're
// adding to this enum with the intention that it will be logged, update the
// AppListOmniboxResult enum listing in tools/metrics/histograms/enums.xml.
enum class OmniboxResultType {
  kQuerySuggestion = 0,
  kZeroStateSuggestion = 1,
  kMaxValue = kZeroStateSuggestion,
};

namespace app_list {

class OmniboxResult : public ChromeSearchResult, public BitmapFetcherDelegate {
 public:
  OmniboxResult(Profile* profile,
                AppListControllerDelegate* list_controller,
                AutocompleteController* autocomplete_controller,
                FaviconCache* favicon_cache,
                const AutocompleteMatch& match,
                bool is_zero_suggestion);
  ~OmniboxResult() override;

  OmniboxResult(const OmniboxResult&) = delete;
  OmniboxResult& operator=(const OmniboxResult&) = delete;

  // ChromeSearchResult:
  void Open(int event_flags) override;
  void InvokeAction(ash::SearchResultActionType action) override;

  // BitmapFetcherDelegate:
  void OnFetchComplete(const GURL& url, const SkBitmap* bitmap) override;

  // Returns the URL that will be navigated to by this search result.
  GURL DestinationURL() const;

 private:
  void UpdateIcon();
  void UpdateTitleAndDetails();

  void Remove();

  // Returns true if |match_| indicates a url search result with non-empty
  // description.
  bool IsUrlResultWithDescription() const;

  void FetchRichEntityImage(const GURL& url);

  void OnFaviconFetched(const gfx::Image& icon);

  void SetZeroSuggestionActions();

  void RecordOmniboxResultHistogram();

  ash::SearchResultType GetSearchResultType() const;

  Profile* profile_;
  AppListControllerDelegate* list_controller_;
  AutocompleteController* autocomplete_controller_;
  FaviconCache* favicon_cache_;
  AutocompleteMatch match_;
  const bool is_zero_suggestion_;
  std::unique_ptr<BitmapFetcher> bitmap_fetcher_;

  base::WeakPtrFactory<OmniboxResult> weak_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_OMNIBOX_RESULT_H_
