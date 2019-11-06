// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/common/omnibox_features.h"

#include "build/build_config.h"

namespace omnibox {

// Feature used to hide the scheme from steady state URLs displayed in the
// toolbar. It is restored during editing.
const base::Feature kHideFileUrlScheme {
  "OmniboxUIExperimentHideFileUrlScheme",
// Android and iOS don't have the File security chip, and therefore still
// need to show the file scheme.
#if defined(OS_ANDROID) || defined(OS_IOS)
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

// Feature used to hide the scheme from steady state URLs displayed in the
// toolbar. It is restored during editing.
const base::Feature kHideSteadyStateUrlScheme{
    "OmniboxUIExperimentHideSteadyStateUrlScheme",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Feature used to hide trivial subdomains from steady state URLs displayed in
// the toolbar. It is restored during editing.
const base::Feature kHideSteadyStateUrlTrivialSubdomains{
    "OmniboxUIExperimentHideSteadyStateUrlTrivialSubdomains",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Feature used to hide the path, query and ref from steady state URLs
// displayed in the toolbar. It is restored during editing.
const base::Feature kHideSteadyStateUrlPathQueryAndRef {
  "OmniboxUIExperimentHideSteadyStateUrlPathQueryAndRef",
#if defined(OS_IOS)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Feature used to undo all omnibox elisions on a single click or focus action.
const base::Feature kOneClickUnelide{"OmniboxOneClickUnelide",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

// This feature simplifies the security indiciator UI for https:// pages.
// The default behavior is to show no verbose text for EV pages. When disabled,
// the verbose EV indicator text will be displayed.
// This feature is used for EV UI removal experiment (https://crbug.com/803501).
const base::Feature kSimplifyHttpsIndicator{"SimplifyHttpsIndicator",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

// This feature is used to have final suggestions within the Omnibox grouped
// by major type. i.e. search types are first, followed by all others,
// except for the default match which is unchanged in position.
const base::Feature kOmniboxGroupSuggestionsBySearchVsUrl{
    "OmniboxGroupSuggestionsBySearchVsUrl", base::FEATURE_DISABLED_BY_DEFAULT};

// Feature used to enable local entity suggestions. Similar to rich entities but
// but location specific. E.g., typing 'starbucks near' could display the local
// entity suggestion 'starbucks near disneyland \n starbucks * Anaheim, CA'.
const base::Feature kOmniboxLocalEntitySuggestions{
    "OmniboxLocalEntitySuggestions", base::FEATURE_DISABLED_BY_DEFAULT};

// Feature used to cap the number of URL-type matches shown within the
// Omnibox. If enabled, the number of URL-type matches is limited (unless
// there are no more non-URL matches available.) If enabled, there is a
// companion parameter - OmniboxMaxURLMatches - which specifies the maximum
// desired number of URL-type matches.
const base::Feature kOmniboxMaxURLMatches{"OmniboxMaxURLMatches",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Feature used to enable entity suggestion images and enhanced presentation
// showing more context and descriptive text about the entity.
const base::Feature kOmniboxRichEntitySuggestions{
    "OmniboxRichEntitySuggestions",
#if defined(OS_IOS) || defined(OS_ANDROID)
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

const base::Feature kOmniboxPreserveDefaultMatchScore{
    "OmniboxPreserveDefaultMatchScore", base::FEATURE_DISABLED_BY_DEFAULT};

// Feature used to enable swapping the rows on answers.
const base::Feature kOmniboxReverseAnswers{"OmniboxReverseAnswers",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Feature used to enable matching short words to bookmarks for suggestions.
const base::Feature kOmniboxShortBookmarkSuggestions{
    "OmniboxShortBookmarkSuggestions", base::FEATURE_DISABLED_BY_DEFAULT};

// Feature used to force on the experiment of transmission of tail suggestions
// from GWS to this client, currently testing for desktop.
const base::Feature kOmniboxTailSuggestions{
    "OmniboxTailSuggestions", base::FEATURE_DISABLED_BY_DEFAULT};

// Feature that enables the tab-switch button on suggestions corresponding to an
// open tab. Enabled by default on Desktop and iOS.
const base::Feature kOmniboxTabSwitchSuggestions{
  "OmniboxTabSwitchSuggestions",
#if defined(OS_ANDROID)
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

// Feature that enables wrapping the Omnibox position between top and bottom.
// The feature is enabled by default, but remains as a kill-switch.
const base::Feature kOmniboxWrapPopupPosition{"OmniboxWrapPopupPosition",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// Feature used to reverse the sense of the tab switch button. Selecting the
// suggestion will switch to the tab, while the button will navigate
// locally.
const base::Feature kOmniboxReverseTabSwitchLogic{
    "OmniboxReverseTabSwitchLogic", base::FEATURE_DISABLED_BY_DEFAULT};

// Feature used to enable various experiments on keyword mode, UI and
// suggestions.
const base::Feature kExperimentalKeywordMode{"OmniboxExperimentalKeywordMode",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// Feature used to enable Pedal suggestions.
const base::Feature kOmniboxPedalSuggestions{"OmniboxPedalSuggestions",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// Feature used for UI that improves transparency of and control over omnibox
// suggestions. This includes UI cues (like a clock icon for Search History
// suggestions), as well as user controls to delete personalized suggestions.
// This will be eventually enabled by default.
const base::Feature kOmniboxSuggestionTransparencyOptions{
    "OmniboxSuggestionTransparencyOptions", base::FEATURE_DISABLED_BY_DEFAULT};

// Feature that shows UI cues to differentiate Search History matches from
// other search suggestions provided by the default search provider. This
// feature is a narrow subset of kOmniboxSuggestionTransparencyOptions.
const base::Feature kOmniboxUICuesForSearchHistoryMatches{
    "OmniboxUICuesForSearchHistoryMatches", base::FEATURE_ENABLED_BY_DEFAULT};

// Feature that shows an alternate separator before the description of
// omnibox matches. In English, this changes the separator from '-' to '|'.
const base::Feature kOmniboxAlternateMatchDescriptionSeparator{
    "OmniboxAlternateMatchDescriptionSeparator",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Feature to enable clipboard provider to suggest copied text.
const base::Feature kEnableClipboardProviderTextSuggestions{
  "OmniboxEnableClipboardProviderTextSuggestions",
#if defined(OS_IOS)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Feature to enable clipboard provider to suggest searching for copied images.
const base::Feature kEnableClipboardProviderImageSuggestions{
  "OmniboxEnableClipboardProviderImageSuggestions",
#if defined(OS_IOS)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Feature to enable the search provider to send a request to the suggest
// server on focus.  This allows the suggest server to warm up, by, for
// example, loading per-user models into memory.  Having a per-user model
// in memory allows the suggest server to respond more quickly with
// personalized suggestions as the user types.
const base::Feature kSearchProviderWarmUpOnFocus{
  "OmniboxWarmUpSearchProviderOnFocus",
#if defined(OS_IOS)
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

// Feature used to display the title of the current URL match.
const base::Feature kDisplayTitleForCurrentUrl{
  "OmniboxDisplayTitleForCurrentUrl",
#if !defined(OS_IOS)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Feature used for the max autocomplete matches UI experiment.
const base::Feature kUIExperimentMaxAutocompleteMatches{
    "OmniboxUIExperimentMaxAutocompleteMatches",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Feature used to display the search terms instead of the URL in the Omnibox
// when the user is on the search results page of the default search provider.
const base::Feature kQueryInOmnibox{"QueryInOmnibox",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// Feature used for showing the URL suggestion favicons as a UI experiment.
// Already launched on Desktop, and currently under development on Android.
// This flag is not used on iOS.
const base::Feature kUIExperimentShowSuggestionFavicons{
  "OmniboxUIExperimentShowSuggestionFavicons",
#if defined(OS_ANDROID)
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

// Feature used to always swap the title and URL.
const base::Feature kUIExperimentSwapTitleAndUrl{
    "OmniboxUIExperimentSwapTitleAndUrl",
#if defined(OS_IOS) || defined(OS_ANDROID)
    base::FEATURE_DISABLED_BY_DEFAULT
#else
    base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

// Feature used for the vertical margin UI experiment, currently only used on
// desktop platforms.
const base::Feature kUIExperimentVerticalMargin{
    "OmniboxUIExperimentVerticalMargin", base::FEATURE_DISABLED_BY_DEFAULT};

// Feature used to limit the vertical margin UI experiment to non-touch
// devices only. Has no effect if kUIExperimentVerticalMargin is not enabled.
const base::Feature kUIExperimentVerticalMarginLimitToNonTouchOnly{
    "OmniboxUIExperimentVerticalMarginLimitToNonTouchOnly",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Shows the "Search Google or type a URL" omnibox placeholder even when the
// caret (text edit cursor) is showing / when focused. views::Textfield works
// this way, as does <input placeholder="">. Omnibox and the NTP's "fakebox"
// are exceptions in this regard and this experiment makes this more consistent.
const base::Feature kUIExperimentShowPlaceholderWhenCaretShowing{
    "OmniboxUIExperimentShowPlaceholderWhenCaretShowing",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Feature used to enable speculatively starting a service worker associated
// with the destination of the default match when the user's input looks like a
// query.
const base::Feature kSpeculativeServiceWorkerStartOnQueryInput{
  "OmniboxSpeculativeServiceWorkerStartOnQueryInput",
      base::FEATURE_ENABLED_BY_DEFAULT
};

// Feature used to fetch document suggestions.
const base::Feature kDocumentProvider{"OmniboxDocumentProvider",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// Feature to replace the standard ZeroSuggest with icons for most visited sites
// and collections (bookmarks, history, recent tabs, reading list). Only
// available on iOS.
const base::Feature kOmniboxPopupShortcutIconsInZeroState{
    "OmniboxPopupShortcutIconsInZeroState", base::FEATURE_DISABLED_BY_DEFAULT};

// Feature to use material design weather icons in the omnibox when displaying
// weather answers.
const base::Feature kOmniboxMaterialDesignWeatherIcons{
    "OmniboxMaterialDesignWeatherIcons", base::FEATURE_DISABLED_BY_DEFAULT};

// Returns whether IsInstantExtendedAPIEnabled should be ignored when deciding
// the number of Google-provided search suggestions.
const base::Feature kOmniboxDisableInstantExtendedLimit{
    "OmniboxDisableInstantExtendedLimit", base::FEATURE_DISABLED_BY_DEFAULT};

// Show the search engine logo in the omnibox on Android (desktop already does
// this).
const base::Feature kOmniboxSearchEngineLogo{"OmniboxSearchEngineLogo",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// Feature to configure on-focus suggestions provided by ZeroSuggestProvider.
// This feature's main job is to contain some field trial parameters such as:
//  - "ZeroSuggestVariant" configures the per-page-classification mode of
//    ZeroSuggestProvider.
//  - "CustomEndpointURL" configures a custom endpoint for remote suggestions.
//  - "CustomEndpointExperimentID" sends an id number to the custom endpoint.
const base::Feature kOnFocusSuggestions{"OmniboxOnFocusSuggestions",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

// Allow suggestions to be shown to the user on the New Tab Page upon focusing
// URL bar (the omnibox).
const base::Feature kZeroSuggestionsOnNTP{"OmniboxZeroSuggestionsOnNTP",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Feature to provide non personalized head search suggestion from a compact
// on device model.
const base::Feature kOnDeviceHeadProvider{"OmniboxOnDeviceHeadProvider",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace omnibox
