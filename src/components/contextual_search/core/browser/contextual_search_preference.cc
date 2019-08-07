// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/core/browser/contextual_search_preference.h"

#include "components/contextual_search/buildflags.h"
#include "components/prefs/pref_service.h"

namespace {

// TODO(donnd): figure out a good way to share these with pref_names.cc!
const char kContextualSearchEnabled[] = "search.contextual_search_enabled";
#if BUILDFLAG(BUILD_CONTEXTUAL_SEARCH)
const char kContextualSearchDisabledValue[] = "false";
const char kContextualSearchEnabledValue[] = "true";
#endif  // BUILDFLAG

}  // namespace

namespace contextual_search {

const char* GetPrefName() {
  return kContextualSearchEnabled;
}

bool IsEnabled(const PrefService& prefs) {
#if BUILDFLAG(BUILD_CONTEXTUAL_SEARCH)
  return prefs.GetString(GetPrefName()) != kContextualSearchDisabledValue;
#else   // BUILDFLAG
  return false;
#endif  // BUILDFLAG
}

ContextualSearchPreference* ContextualSearchPreference::GetInstance() {
  static base::NoDestructor<ContextualSearchPreference> instance;
  return instance.get();
}

ContextualSearchPreference::ContextualSearchPreference()
    : previous_preference_metadata_(UNKNOWN) {}

ContextualSearchPreviousPreferenceMetadata
ContextualSearchPreference::GetPreviousPreferenceMetadata() {
  return previous_preference_metadata_;
}

void ContextualSearchPreference::SetPref(PrefService* pref_service,
                                         bool enable) {
// Ignore the pref change if the feature is not available on this platform.
#if BUILDFLAG(BUILD_CONTEXTUAL_SEARCH)
  pref_service->SetString(
      kContextualSearchEnabled,
      enable ? kContextualSearchEnabledValue : kContextualSearchDisabledValue);
#endif  // BUILDFLAG
}

void ContextualSearchPreference::EnableIfUndecided(PrefService* pref_service) {
#if BUILDFLAG(BUILD_CONTEXTUAL_SEARCH)
  previous_preference_metadata_ = pref_service->GetString(GetPrefName()) == ""
                                      ? WAS_UNDECIDED
                                      : WAS_DECIDED;
  if (previous_preference_metadata_ == WAS_UNDECIDED)
    SetPref(pref_service, true);
#endif  // BUILDFLAG
}

}  // namespace contextual_search
