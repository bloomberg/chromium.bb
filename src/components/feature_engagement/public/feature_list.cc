// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/public/feature_list.h"

#include "base/cxx17_backports.h"
#include "build/build_config.h"
#include "components/feature_engagement/public/feature_constants.h"

namespace feature_engagement {

namespace {
// Whenever a feature is added to |kAllFeatures|, it should also be added as
// DEFINE_VARIATION_PARAM in the header, and also added to the
// |kIPHDemoModeChoiceVariations| array.
const base::Feature* const kAllFeatures[] = {
    &kIPHDummyFeature,  // Ensures non-empty array for all platforms.
#if BUILDFLAG(IS_ANDROID)
    &kIPHAdaptiveButtonInTopToolbarCustomizationNewTabFeature,
    &kIPHAdaptiveButtonInTopToolbarCustomizationShareFeature,
    &kIPHAdaptiveButtonInTopToolbarCustomizationVoiceSearchFeature,
    &kIPHAddToHomescreenMessageFeature,
    &kIPHAddToHomescreenTextBubbleFeature,
    &kIPHAutoDarkOptOutFeature,
    &kIPHAutoDarkUserEducationMessageFeature,
    &kIPHAutoDarkUserEducationMessageOptInFeature,
    &kIPHDataSaverDetailFeature,
    &kIPHDataSaverMilestonePromoFeature,
    &kIPHDataSaverPreviewFeature,
    &kIPHDownloadHomeFeature,
    &kIPHDownloadIndicatorFeature,
    &kIPHDownloadPageFeature,
    &kIPHDownloadPageScreenshotFeature,
    &kIPHChromeHomeExpandFeature,
    &kIPHChromeHomePullToRefreshFeature,
    &kIPHChromeReengagementNotification1Feature,
    &kIPHChromeReengagementNotification2Feature,
    &kIPHChromeReengagementNotification3Feature,
    &kIPHContextualSearchTranslationEnableFeature,
    &kIPHContextualSearchWebSearchFeature,
    &kIPHContextualSearchPromoteTapFeature,
    &kIPHContextualSearchPromotePanelOpenFeature,
    &kIPHContextualSearchOptInFeature,
    &kIPHContextualSearchTappedButShouldLongpressFeature,
    &kIPHContextualSearchInPanelHelpFeature,
    &kIPHDownloadSettingsFeature,
    &kIPHDownloadInfoBarDownloadContinuingFeature,
    &kIPHDownloadInfoBarDownloadsAreFasterFeature,
    &kIPHEphemeralTabFeature,
    &kIPHFeatureNotificationGuideDefaultBrowserNotificationShownFeature,
    &kIPHFeatureNotificationGuideSignInNotificationShownFeature,
    &kIPHFeatureNotificationGuideIncognitoTabNotificationShownFeature,
    &kIPHFeatureNotificationGuideNTPSuggestionCardNotificationShownFeature,
    &kIPHFeatureNotificationGuideVoiceSearchNotificationShownFeature,
    &kIPHFeatureNotificationGuideDefaultBrowserPromoFeature,
    &kIPHFeatureNotificationGuideSignInHelpBubbleFeature,
    &kIPHFeatureNotificationGuideIncognitoTabHelpBubbleFeature,
    &kIPHFeatureNotificationGuideVoiceSearchHelpBubbleFeature,
    &kIPHFeatureNotificationGuideNTPSuggestionCardHelpBubbleFeature,
    &kIPHFeatureNotificationGuideIncognitoTabUsedFeature,
    &kIPHFeatureNotificationGuideVoiceSearchUsedFeature,
    &kIPHFeedCardMenuFeature,
    &kIPHGenericAlwaysTriggerHelpUiFeature,
    &kIPHHomepagePromoCardFeature,
    &kIPHIdentityDiscFeature,
    &kIPHInstanceSwitcherFeature,
    &kIPHKeyboardAccessoryAddressFillingFeature,
    &kIPHKeyboardAccessoryBarSwipingFeature,
    &kIPHKeyboardAccessoryPasswordFillingFeature,
    &kIPHKeyboardAccessoryPaymentFillingFeature,
    &kIPHKeyboardAccessoryPaymentOfferFeature,
    &kIPHLowUserEngagementDetectorFeature,
    &kIPHMicToolbarFeature,
    &kIPHNewTabPageHomeButtonFeature,
    &kIPHPageInfoFeature,
    &kIPHPageInfoStoreInfoFeature,
    &kIPHPreviewsOmniboxUIFeature,
    &kIPHPwaInstallAvailableFeature,
    &kIPHQuietNotificationPromptsFeature,
    &kIPHReadLaterContextMenuFeature,
    &kIPHReadLaterAppMenuBookmarkThisPageFeature,
    &kIPHReadLaterAppMenuBookmarksFeature,
    &kIPHReadLaterBottomSheetFeature,
    &kIPHShoppingListMenuItemFeature,
    &kIPHShoppingListSaveFlowFeature,
    &kIPHTabGroupsQuicklyComparePagesFeature,
    &kIPHTabGroupsTapToSeeAnotherTabFeature,
    &kIPHTabGroupsYourTabsAreTogetherFeature,
    &kIPHTabGroupsDragAndDropFeature,
    &kIPHTabSwitcherButtonFeature,
    &kIPHTranslateMenuButtonFeature,
    &kIPHVideoTutorialNTPChromeIntroFeature,
    &kIPHVideoTutorialNTPDownloadFeature,
    &kIPHVideoTutorialNTPSearchFeature,
    &kIPHVideoTutorialNTPVoiceSearchFeature,
    &kIPHVideoTutorialNTPSummaryFeature,
    &kIPHVideoTutorialTryNowFeature,
    &kIPHExploreSitesTileFeature,
    &kIPHFeedHeaderMenuFeature,
    &kIPHFeedSwipeRefresh,
    &kIPHShareScreenshotFeature,
    &kIPHSharingHubLinkToggleFeature,
    &kIPHWebFeedFollowFeature,
    &kIPHWebFeedPostFollowDialogFeature,
    &kIPHSharedHighlightingBuilder,
    &kIPHSharedHighlightingReceiverFeature,
    &kIPHStartSurfaceTabSwitcherHomeButton,
    &kIPHUpdatedConnectionSecurityIndicatorsFeature,
    &kIPHSharingHubWebnotesStylizeFeature,
#endif  // BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(IS_IOS)
    &kIPHBottomToolbarTipFeature,
    &kIPHLongPressToolbarTipFeature,
    &kIPHNewTabTipFeature,
    &kIPHNewIncognitoTabTipFeature,
    &kIPHBadgedReadingListFeature,
    &kIPHReadingListMessagesFeature,
    &kIPHBadgedTranslateManualTriggerFeature,
    &kIPHDiscoverFeedHeaderFeature,
    &kIPHDefaultSiteViewFeature,
#endif  // BUILDFLAG(IS_IOS)
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
    &kIPHDesktopTabGroupsNewGroupFeature,
    &kIPHFocusHelpBubbleScreenReaderPromoFeature,
    &kIPHGMCCastStartStopFeature,
    &kIPHLiveCaptionFeature,
    &kIPHTabAudioMutingFeature,
    &kIPHPasswordsAccountStorageFeature,
    &kIPHReadingListDiscoveryFeature,
    &kIPHReadingListEntryPointFeature,
    &kIPHReadingListInSidePanelFeature,
    &kIPHReopenTabFeature,
    &kIPHSideSearchFeature,
    &kIPHTabSearchFeature,
    &kIPHWebUITabStripFeature,
    &kIPHDesktopPwaInstallFeature,
    &kIPHProfileSwitchFeature,
    &kIPHUpdatedConnectionSecurityIndicatorsFeature,
    &kIPHDesktopSharedHighlightingFeature,
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA)
    &kIPHAutofillVirtualCardSuggestionFeature,
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) ||
        // BUILDFLAG(IS_FUCHSIA)
};
}  // namespace

const char kIPHDemoModeFeatureChoiceParam[] = "chosen_feature";

std::vector<const base::Feature*> GetAllFeatures() {
  return std::vector<const base::Feature*>(
      kAllFeatures, kAllFeatures + base::size(kAllFeatures));
}

}  // namespace feature_engagement
