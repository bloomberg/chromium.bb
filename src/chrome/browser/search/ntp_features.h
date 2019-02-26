// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_NTP_FEATURES_H_
#define CHROME_BROWSER_SEARCH_NTP_FEATURES_H_

#include "base/feature_list.h"

namespace features {

// The features should be documented alongside the definition of their values in
// the .cc file.

extern const base::Feature kDoodlesOnLocalNtp;
extern const base::Feature kNtpBackgrounds;
extern const base::Feature kNtpIcons;
extern const base::Feature kNtpUIMd;
extern const base::Feature kPromosOnLocalNtp;
extern const base::Feature kSearchSuggestionsOnLocalNtp;
extern const base::Feature kUseGoogleLocalNtp;

// Returns whether New Tab Page custom links are enabled.
bool IsCustomLinksEnabled();

// Returns whether New Tab Page Background Selection is enabled.
bool IsCustomBackgroundsEnabled();

// Returns whether the Material Design UI for Most Visited is enabled.
bool IsMDIconsEnabled();

// Returns whether the Material Design UI is enabled on the New Tab Page.
bool IsMDUIEnabled();

}  // namespace features

#endif  // CHROME_BROWSER_SEARCH_NTP_FEATURES_H_
