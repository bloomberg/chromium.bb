// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/common/omnibox_features.h"

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
const base::Feature kHideSteadyStateUrlScheme {
  "OmniboxUIExperimentHideSteadyStateUrlScheme",
#if defined(OS_IOS)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Feature used to hide trivial subdomains from steady state URLs displayed in
// the toolbar. It is restored during editing.
const base::Feature kHideSteadyStateUrlTrivialSubdomains {
  "OmniboxUIExperimentHideSteadyStateUrlTrivialSubdomains",
#if defined(OS_IOS)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

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

// This feature simplifies the security indiciator UI for https:// pages. The
// exact UI treatment is dependent on the parameter 'treatment' which can have
// the following value:
// - 'ev-to-secure': Show the "Secure" chip for pages with an EV certificate.
// - 'secure-to-lock': Show only the lock icon for non-EV https:// pages.
// - 'both-to-lock': Show only the lock icon for all https:// pages.
// - 'keep-secure-chip': Show the old "Secure" chip for non-EV https:// pages.
// The default behavior is the same as 'secure-to-lock'.
// This feature is used for EV UI removal experiment (https://crbug.com/803501).
const base::Feature kSimplifyHttpsIndicator{"SimplifyHttpsIndicator",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

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

// Feature used to enable enhanced presentation showing larger images.
// This is currently only used on Android.
const base::Feature kOmniboxNewAnswerLayout{"OmniboxNewAnswerLayout",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

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
    "OmniboxUICuesForSearchHistoryMatches", base::FEATURE_DISABLED_BY_DEFAULT};

// Feature that shows an alternate separator before the description of
// omnibox matches. In English, this changes the separator from '-' to '|'.
const base::Feature kOmniboxAlternateMatchDescriptionSeparator{
    "OmniboxAlternateMatchDescriptionSeparator",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Feature to enable clipboard provider to suggest copied text.
const base::Feature kEnableClipboardProviderTextSuggestions{
    "OmniboxEnableClipboardProviderTextSuggestions",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Feature to enable clipboard provider to suggest searching for copied images.
const base::Feature kEnableClipboardProviderImageSuggestions{
    "OmniboxEnableClipboardProviderImageSuggestions",
    base::FEATURE_DISABLED_BY_DEFAULT};

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

// Feature used to dedupe Google Drive URLs between different formats.
// OmniboxDocumentProvider arms may wish to enable this, though it may also be
// run on its own.
const base::Feature kDedupeGoogleDriveURLs{"OmniboxDedupeGoogleDriveURLs",
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

// Feature to configure on-focus suggestions provided by ZeroSuggestProvider.
// This feature's main job is to contain the "ZeroSuggestVariant" field trial
// parameter, which configures the global mode of ZeroSuggestProvider.
const base::Feature kOnFocusSuggestions{"OmniboxOnFocusSuggestions",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

// Feature used for the Zero Suggest Redirect to Chrome Field Trial.
//
// This feature is *enabled* in order to *disable* all forms of suggestions
// based on the URL on-focus (whether from "redirect to Chrome" or the
// default suggest server).  The actual disabling of redirect to Chrome
// suggestions happens in contextual_suggestions_service.cc.  See comments
// by kDefaultExperimentalServerAddress.
//
// If this feature were not enabled, Chrome would use the default suggest
// server for suggestions based on the current URL on focus.  There is no
// code in Chrome to disable that, so that why we took this route.
const base::Feature kZeroSuggestRedirectToChrome{
    "ZeroSuggestRedirectToChrome", base::FEATURE_ENABLED_BY_DEFAULT};

// Allow suggestions to be shown to the user on the New Tab Page upon focusing
// URL bar (the omnibox).
const base::Feature kZeroSuggestionsOnNTP{"OmniboxZeroSuggestionsOnNTP",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace omnibox
