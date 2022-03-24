// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_PROVIDER_CLIENT_H_
#define COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_PROVIDER_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "components/history/core/browser/keyword_id.h"
#include "components/history/core/browser/top_sites.h"
#include "components/omnibox/browser/actions/omnibox_action.h"
#include "components/omnibox/browser/keyword_extensions_delegate.h"
#include "components/omnibox/browser/omnibox_triggered_feature_service.h"
#include "components/omnibox/browser/shortcuts_backend.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

class AutocompleteController;
struct AutocompleteMatch;
class AutocompleteClassifier;
class AutocompleteSchemeClassifier;
class RemoteSuggestionsService;
class DocumentSuggestionsService;
class GURL;
class InMemoryURLIndex;
class KeywordProvider;
class OmniboxPedalProvider;
class PrefService;
class ShortcutsBackend;
class TabMatcher;

namespace bookmarks {
class BookmarkModel;
}

namespace history {
class HistoryService;
class URLDatabase;
}

namespace history_clusters {
class HistoryClustersService;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace component_updater {
class ComponentUpdateService;
}

namespace signin {
class IdentityManager;
}

namespace query_tiles {
class TileService;
}

namespace ntp_tiles {
class MostVisitedSites;
}

class TemplateURLService;

class AutocompleteProviderClient : public OmniboxAction::Client {
 public:
  virtual ~AutocompleteProviderClient() {}

  virtual scoped_refptr<network::SharedURLLoaderFactory>
  GetURLLoaderFactory() = 0;
  virtual PrefService* GetPrefs() const = 0;
  virtual PrefService* GetLocalState() = 0;
  virtual const AutocompleteSchemeClassifier& GetSchemeClassifier() const = 0;
  virtual AutocompleteClassifier* GetAutocompleteClassifier() = 0;
  virtual history::HistoryService* GetHistoryService() = 0;
  virtual history_clusters::HistoryClustersService* GetHistoryClustersService();
  virtual scoped_refptr<history::TopSites> GetTopSites() = 0;
  virtual ntp_tiles::MostVisitedSites* GetNtpMostVisitedSites();
  virtual bookmarks::BookmarkModel* GetBookmarkModel() = 0;
  virtual history::URLDatabase* GetInMemoryDatabase() = 0;
  virtual InMemoryURLIndex* GetInMemoryURLIndex() = 0;
  virtual TemplateURLService* GetTemplateURLService() = 0;
  virtual const TemplateURLService* GetTemplateURLService() const = 0;
  virtual RemoteSuggestionsService* GetRemoteSuggestionsService(
      bool create_if_necessary) const = 0;
  virtual DocumentSuggestionsService* GetDocumentSuggestionsService(
      bool create_if_necessary) const = 0;
  virtual OmniboxPedalProvider* GetPedalProvider() const = 0;
  virtual scoped_refptr<ShortcutsBackend> GetShortcutsBackend() = 0;
  virtual scoped_refptr<ShortcutsBackend> GetShortcutsBackendIfExists() = 0;
  virtual std::unique_ptr<KeywordExtensionsDelegate>
  GetKeywordExtensionsDelegate(KeywordProvider* keyword_provider) = 0;
  virtual query_tiles::TileService* GetQueryTileService() const = 0;
  virtual OmniboxTriggeredFeatureService* GetOmniboxTriggeredFeatureService()
      const = 0;

  // The value to use for Accept-Languages HTTP header when making an HTTP
  // request.
  virtual std::string GetAcceptLanguages() const = 0;

  // The embedder's representation of the |about| URL scheme for builtin URLs
  // (e.g., |chrome| for Chrome).
  virtual std::string GetEmbedderRepresentationOfAboutScheme() const = 0;

  // The set of built-in URLs considered worth suggesting as autocomplete
  // suggestions to the user.  Some built-in URLs, e.g. hidden URLs that
  // intentionally crash the product for testing purposes, may be omitted from
  // this list if suggesting them is undesirable.
  virtual std::vector<std::u16string> GetBuiltinURLs() = 0;

  // The set of URLs to provide as autocomplete suggestions as the user types a
  // prefix of the |about| scheme or the embedder's representation of that
  // scheme. Note that this may be a subset of GetBuiltinURLs(), e.g., only the
  // most commonly-used URLs from that set.
  virtual std::vector<std::u16string> GetBuiltinsToProvideAsUserTypes() = 0;

  // TODO(crbug/925072): clean up component update service if it's confirmed
  // it's not needed for on device head provider.
  // The component update service instance which will be used by on device
  // suggestion provider to observe the model update event.
  virtual component_updater::ComponentUpdateService*
  GetComponentUpdateService() = 0;

  // Returns the signin::IdentityManager associated with the current profile.
  virtual signin::IdentityManager* GetIdentityManager() const = 0;

  virtual bool IsOffTheRecord() const = 0;
  virtual bool SearchSuggestEnabled() const = 0;

  // True for almost all users except ones with a specific enterprise policy.
  virtual bool AllowDeletingBrowserHistory() const;

  // Returns whether personalized URL data collection is enabled.  I.e.,
  // the user has consented to have URLs recorded keyed by their Google account.
  // In this case, the user has agreed to share browsing data with Google and so
  // this state can be used to govern features such as sending the current page
  // URL with omnibox suggest requests.
  virtual bool IsPersonalizedUrlDataCollectionActive() const = 0;

  // This function returns true if the user is signed in.
  virtual bool IsAuthenticated() const = 0;

  // Determines whether sync is enabled.
  virtual bool IsSyncActive() const = 0;

  virtual std::string ProfileUserName() const;

  // Given some string |text| that the user wants to use for navigation,
  // determines how it should be interpreted.
  virtual void Classify(
      const std::u16string& text,
      bool prefer_keyword,
      bool allow_exact_keyword_match,
      metrics::OmniboxEventProto::PageClassification page_classification,
      AutocompleteMatch* match,
      GURL* alternate_nav_url) = 0;

  // Deletes all URL and search term entries matching the given |term| and
  // |keyword_id| from history.
  virtual void DeleteMatchingURLsForKeywordFromHistory(
      history::KeywordID keyword_id,
      const std::u16string& term) = 0;

  virtual void PrefetchImage(const GURL& url) = 0;

  // Sends a hint to the service worker context that navigation to
  // |desination_url| is likely, unless the current profile is in incognito
  // mode. On platforms where this is supported, the service worker lookup can
  // be expensive so this method should only be called once per input session.
  virtual void StartServiceWorker(const GURL& destination_url) {}

  // Called by |controller| when its results have changed and all providers are
  // done processing the autocomplete request. Used by chrome to inform the
  // prefetch service of updated results.
  virtual void OnAutocompleteControllerResultReady(
      AutocompleteController* controller) {}

  // Called after creation of |keyword_provider| to allow the client to
  // configure the provider if desired.
  virtual void ConfigureKeywordProvider(KeywordProvider* keyword_provider) {}

  // Called to acquire the instance of TabMatcher, used to identify open tabs
  // for a given set of AutocompleteMatches within the current profile.
  virtual const TabMatcher& GetTabMatcher() const = 0;

  // Returns whether user is currently allowed to enter incognito mode.
  virtual bool IsIncognitoModeAvailable() const;

  // Returns true if the sharing hub command is enabled.
  virtual bool IsSharingHubAvailable() const;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_PROVIDER_CLIENT_H_
