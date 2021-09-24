// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_provider_client.h"

history_clusters::HistoryClustersService*
AutocompleteProviderClient::GetHistoryClustersService() {
  return nullptr;
}

ntp_tiles::MostVisitedSites*
AutocompleteProviderClient::GetNtpMostVisitedSites() {
  return nullptr;
}

bool AutocompleteProviderClient::AllowDeletingBrowserHistory() const {
  return true;
}

std::string AutocompleteProviderClient::ProfileUserName() const {
  return "";
}

bool AutocompleteProviderClient::IsIncognitoModeAvailable() const {
  return true;
}

bool AutocompleteProviderClient::IsSharingHubAvailable() const {
  return false;
}
