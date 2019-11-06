// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_UTIL_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_UTIL_H_

#import <UIKit/UIKit.h>

#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/security_state/core/security_state.h"
#include "ios/chrome/browser/ui/omnibox/omnibox_suggestion_icon_util.h"

#pragma mark - Suggestion icons.

// Converts |type| to the appropriate icon type for this match type to show in
// the omnibox.
OmniboxSuggestionIconType GetOmniboxSuggestionIconTypeForAutocompleteMatchType(
    AutocompleteMatchType::Type type,
    bool is_starred);

// Converts |type| to the appropriate icon for this type to show in the omnibox.
// Returns UI Refresh icons.
UIImage* GetOmniboxSuggestionIconForAutocompleteMatchType(
    AutocompleteMatchType::Type type,
    bool is_starred);

#pragma mark - Security icons.

// All available icons for security states.
enum LocationBarSecurityIconType {
  INSECURE = 0,
  SECURE,
  DANGEROUS,
  LOCATION_BAR_SECURITY_ICON_TYPE_COUNT,
};

// Returns the asset name (to be used in -[UIImage imageNamed:]).
NSString* GetLocationBarSecurityIconTypeAssetName(
    LocationBarSecurityIconType icon);

// Returns the asset with "always template" rendering mode.
UIImage* GetLocationBarSecurityIcon(LocationBarSecurityIconType icon);

// Converts the |security_level| to an appropriate security icon type.
LocationBarSecurityIconType GetLocationBarSecurityIconTypeForSecurityState(
    security_state::SecurityLevel security_level);

// Converts the |security_level| to an appropriate icon in "always template"
// rendering mode.
UIImage* GetLocationBarSecurityIconForSecurityState(
    security_state::SecurityLevel security_level);

#pragma mark - Legacy utils.

// Converts |type| to a resource identifier for the appropriate icon for this
// type to show in the omnibox.
int GetIconForAutocompleteMatchType(AutocompleteMatchType::Type type,
                                    bool is_starred,
                                    bool is_incognito);


// Converts |security_level| to a resource identifier for the appropriate icon
// for this security level in the omnibox.
int GetIconForSecurityState(security_state::SecurityLevel security_level);

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_UTIL_H_
