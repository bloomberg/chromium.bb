// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/most_visited_sites_provider.h"

#include <string>

#include "base/bind.h"
#include "base/feature_list.h"
#include "components/history/core/browser/top_sites.h"
#include "components/ntp_tiles/most_visited_sites.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/url_formatter/url_formatter.h"
#include "net/base/escape.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_input_type.pb.h"
#include "url/gurl.h"

namespace {
// The relevance score for navsuggest tiles.
// Navsuggest tiles should be positioned below the Query Tiles object.
constexpr const int kMostVisitedTilesRelevance = 1500;

// Use the same max number of tiles as MostVisitedListCoordinator to offer the
// same content.
constexpr const int kMaxTileCount = 12;

// Constructs an AutocompleteMatch from supplied details.
AutocompleteMatch BuildMatch(AutocompleteProvider* provider,
                             AutocompleteProviderClient* client,
                             const std::u16string& description,
                             const GURL& url,
                             int relevance,
                             AutocompleteMatchType::Type type) {
  AutocompleteMatch match(provider, relevance, false, type);
  match.destination_url = url;

  match.fill_into_edit +=
      AutocompleteInput::FormattedStringWithEquivalentMeaning(
          url, url_formatter::FormatUrl(url), client->GetSchemeClassifier(),
          nullptr);

  // Zero suggest results should always omit protocols and never appear bold.
  auto format_types = AutocompleteMatch::GetFormatTypes(false, false);
  match.contents = url_formatter::FormatUrl(
      url, format_types, net::UnescapeRule::SPACES, nullptr, nullptr, nullptr);
  match.contents_class = ClassifyTermMatches({}, match.contents.length(), 0,
                                             ACMatchClassification::URL);

  match.description = AutocompleteMatch::SanitizeString(description);
  match.description_class = ClassifyTermMatches({}, match.description.length(),
                                                0, ACMatchClassification::NONE);

  return match;
}

template <typename TileContainer>
bool BuildTileNavsuggest(AutocompleteProvider* provider,
                         AutocompleteProviderClient* const client,
                         const TileContainer& container,
                         ACMatches& matches) {
  if (container.empty())
    return false;

  // We force to build TILE_NAVSUGGEST when the TileContainer type is
  // ntp_tiles::NTPTilesVector. This is because:
  // 1) NTPTiles are always presented as a TILE_NAVSUGGEST entry;
  // 2) NTPTiles are only served in the START_SURFACE_HOMEPAGE and
  //    START_SURFACE_NEW_TAB context, making these controlled by the same finch
  //    feature flag as start surface itself.
  bool using_ntp_tiles =
      std::is_same<TileContainer, ntp_tiles::NTPTilesVector>::value;
  if (using_ntp_tiles ||
      base::FeatureList::IsEnabled(omnibox::kMostVisitedTiles)) {
    AutocompleteMatch match = BuildMatch(
        provider, client, std::u16string(), GURL::EmptyGURL(),
        kMostVisitedTilesRelevance, AutocompleteMatchType::TILE_NAVSUGGEST);

    match.navsuggest_tiles.reserve(container.size());

    for (const auto& tile : container) {
      match.navsuggest_tiles.push_back({tile.url, tile.title});
    }
    matches.push_back(std::move(match));
  } else {
    int relevance = 600;
    for (const auto& tile : container) {
      matches.emplace_back(BuildMatch(provider, client, tile.title, tile.url,
                                      relevance,
                                      AutocompleteMatchType::NAVSUGGEST));
      --relevance;
    }
  }
  return true;
}

}  // namespace

void MostVisitedSitesProvider::Start(const AutocompleteInput& input,
                                     bool minimal_changes) {
  Stop(true, false);
  if (!AllowMostVisitedSitesSuggestions(input))
    return;

  if (input.current_page_classification() ==
          metrics::OmniboxEventProto::START_SURFACE_HOMEPAGE ||
      input.current_page_classification() ==
          metrics::OmniboxEventProto::START_SURFACE_NEW_TAB) {
    StartFetchNTPTiles();
    return;
  }

  StartFetchTopSites();
}

void MostVisitedSitesProvider::StartFetchTopSites() {
  scoped_refptr<history::TopSites> top_sites = client_->GetTopSites();
  if (!top_sites)
    return;

  top_sites->GetMostVisitedURLs(
      base::BindRepeating(&MostVisitedSitesProvider::OnMostVisitedUrlsAvailable,
                          request_weak_ptr_factory_.GetWeakPtr()));
}

void MostVisitedSitesProvider::StartFetchNTPTiles() {
  if (!most_visited_sites_)
    most_visited_sites_ = client_->GetNtpMostVisitedSites();

  // |most_visited_sites| will notify the provider when the fetch is complete.
  most_visited_sites_->AddMostVisitedURLsObserver(this, kMaxTileCount);
}

void MostVisitedSitesProvider::Stop(bool clear_cached_results,
                                    bool due_to_user_inactivity) {
  if (most_visited_sites_)
    most_visited_sites_->RemoveMostVisitedURLsObserver(this);

  request_weak_ptr_factory_.InvalidateWeakPtrs();
  if (clear_cached_results)
    matches_.clear();
}

MostVisitedSitesProvider::MostVisitedSitesProvider(
    AutocompleteProviderClient* client,
    AutocompleteProviderListener* listener)
    : AutocompleteProvider(TYPE_MOST_VISITED_SITES),
      client_{client},
      listener_{listener} {}

MostVisitedSitesProvider::~MostVisitedSitesProvider() = default;

void MostVisitedSitesProvider::OnMostVisitedUrlsAvailable(
    const history::MostVisitedURLList& urls) {
  if (BuildTileNavsuggest(this, client_, urls, matches_))
    listener_->OnProviderUpdate(true);
}

bool MostVisitedSitesProvider::AllowMostVisitedSitesSuggestions(
    const AutocompleteInput& input) const {
  const auto& page_url = input.current_url();
  const auto page_class = input.current_page_classification();
  const auto input_type = input.type();

  if (input.focus_type() == OmniboxFocusType::DEFAULT)
    return false;

  if (client_->IsOffTheRecord())
    return false;

  // Check whether current context is one that supports MV tiles.
  // Any context other than those listed below will be rejected.
  if (page_class != metrics::OmniboxEventProto::OTHER &&
      page_class != metrics::OmniboxEventProto::ANDROID_SEARCH_WIDGET &&
      page_class != metrics::OmniboxEventProto::ANDROID_SHORTCUTS_WIDGET &&
      page_class != metrics::OmniboxEventProto::START_SURFACE_HOMEPAGE &&
      page_class != metrics::OmniboxEventProto::START_SURFACE_NEW_TAB) {
    return false;
  }

  // When omnibox contains pre-populated content, only show zero suggest for
  // pages with URLs the user will recognize.
  //
  // This list intentionally does not include items such as ftp: and file:
  // because (a) these do not work on Android and iOS, where most visited
  // zero suggest is launched and (b) on desktop, where contextual zero suggest
  // is running, these types of schemes aren't eligible to be sent to the
  // server to ask for suggestions (and thus in practice we won't display zero
  // suggest for them).
  if (input_type != metrics::OmniboxInputType::EMPTY &&
      !(page_url.is_valid() &&
        ((page_url.scheme() == url::kHttpScheme) ||
         (page_url.scheme() == url::kHttpsScheme) ||
         (page_url.scheme() == url::kAboutScheme) ||
         (page_url.scheme() ==
          client_->GetEmbedderRepresentationOfAboutScheme())))) {
    return false;
  }

  return true;
}

void MostVisitedSitesProvider::OnURLsAvailable(
    const std::map<ntp_tiles::SectionType, ntp_tiles::NTPTilesVector>&
        sections) {
  // If the |matches_| has been build, don't build it again.
  if (!matches_.empty())
    return;

  if (BuildTileNavsuggest(this, client_,
                          sections.at(ntp_tiles::SectionType::PERSONALIZED),
                          matches_)) {
    listener_->OnProviderUpdate(true);
  }
}

void MostVisitedSitesProvider::OnIconMadeAvailable(const GURL& site_url) {}
