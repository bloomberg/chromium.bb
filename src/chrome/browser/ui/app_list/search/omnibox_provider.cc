// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/omnibox_provider.h"

#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/search/omnibox_result.h"
#include "components/favicon/core/favicon_service.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/search_engines/omnibox_focus_type.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "url/gurl.h"

namespace app_list {
namespace {

bool IsDriveUrl(const GURL& url) {
  // Returns true if the |url| points to a Drive Web host.
  const std::string& host = url.host();
  return host == "drive.google.com" || host == "docs.google.com";
}

int ProviderTypes() {
  // We use all the default providers except for the document provider, which
  // suggests Drive files on enterprise devices. This is disabled to avoid
  // duplication with search results from DriveFS.
  return AutocompleteClassifier::DefaultOmniboxProviders() &
         ~AutocompleteProvider::TYPE_DOCUMENT;
}

}  //  namespace

OmniboxProvider::OmniboxProvider(Profile* profile,
                                 AppListControllerDelegate* list_controller)
    : profile_(profile),
      list_controller_(list_controller),
      controller_(std::make_unique<AutocompleteController>(
          std::make_unique<ChromeAutocompleteProviderClient>(profile),
          ProviderTypes())),
      favicon_cache_(FaviconServiceFactory::GetForProfile(
                         profile,
                         ServiceAccessType::EXPLICIT_ACCESS),
                     HistoryServiceFactory::GetForProfile(
                         profile,
                         ServiceAccessType::EXPLICIT_ACCESS)) {
  controller_->AddObserver(this);
}

OmniboxProvider::~OmniboxProvider() {}

void OmniboxProvider::Start(const std::u16string& query) {
  controller_->Stop(false);
  // The new page classification value(CHROMEOS_APP_LIST) is introduced
  // to differentiate the suggest requests initiated by ChromeOS app_list from
  // the ones by Chrome omnibox.
  AutocompleteInput input =
      AutocompleteInput(query, metrics::OmniboxEventProto::CHROMEOS_APP_LIST,
                        ChromeAutocompleteSchemeClassifier(profile_));

  // Sets the |from_omnibox_focus| flag to enable ZeroSuggestProvider to process
  // the requests from app_list.
  if (input.text().empty()) {
    input.set_focus_type(OmniboxFocusType::ON_FOCUS);
    is_zero_state_input_ = true;
  } else {
    is_zero_state_input_ = false;
  }

  query_start_time_ = base::TimeTicks::Now();
  controller_->Start(input);
}

ash::AppListSearchResultType OmniboxProvider::ResultType() {
  return ash::AppListSearchResultType::kOmnibox;
}

void OmniboxProvider::PopulateFromACResult(const AutocompleteResult& result) {
  SearchProvider::Results new_results;
  new_results.reserve(result.size());
  for (const AutocompleteMatch& match : result) {
    // Do not return a match in any of these cases:
    // - The URL is invalid.
    // - The URL points to Drive Web. The Drive search and zero-state
    //   providers surface Drive results.
    // - The URL points to a local file. The Local file search and zero-state
    //   providers handle local file results, even if they've been opened in
    //   the browser.
    if (!match.destination_url.is_valid() ||
        IsDriveUrl(match.destination_url) ||
        match.destination_url.SchemeIsFile()) {
      continue;
    }

    new_results.emplace_back(std::make_unique<OmniboxResult>(
        profile_, list_controller_, controller_.get(), &favicon_cache_, match,
        is_zero_state_input_));
  }

  SwapResults(&new_results);
}

void OmniboxProvider::OnResultChanged(AutocompleteController* controller,
                                      bool default_match_changed) {
  DCHECK(controller == controller_.get());

  // Record the query latency.
  RecordQueryLatencyHistogram();

  PopulateFromACResult(controller_->result());
}

void OmniboxProvider::RecordQueryLatencyHistogram() {
  base::TimeDelta query_latency = base::TimeTicks::Now() - query_start_time_;
  if (is_zero_state_input_) {
    UMA_HISTOGRAM_TIMES("Apps.AppList.OmniboxProvider.ZeroStateLatency",
                        query_latency);
  } else {
    UMA_HISTOGRAM_TIMES("Apps.AppList.OmniboxProvider.QueryTime",
                        query_latency);
  }
}

}  // namespace app_list
