// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.feature_engagement;

import androidx.annotation.StringDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * FeatureConstants contains the String name of all base::Feature in-product help features declared
 * in //components/feature_engagement/public/feature_constants.h.
 */
@StringDef({FeatureConstants.ADD_TO_HOMESCREEN_MESSAGE_FEATURE,
        FeatureConstants.ADD_TO_HOMESCREEN_TEXT_BUBBLE_FEATURE,
        FeatureConstants.DOWNLOAD_PAGE_FEATURE, FeatureConstants.DOWNLOAD_PAGE_SCREENSHOT_FEATURE,
        FeatureConstants.DOWNLOAD_HOME_FEATURE, FeatureConstants.DOWNLOAD_INDICATOR_FEATURE,
        FeatureConstants.CHROME_HOME_EXPAND_FEATURE,
        FeatureConstants.CHROME_HOME_PULL_TO_REFRESH_FEATURE,
        FeatureConstants.DATA_SAVER_PREVIEW_FEATURE, FeatureConstants.DATA_SAVER_DETAIL_FEATURE,
        FeatureConstants.EPHEMERAL_TAB_FEATURE, FeatureConstants.PREVIEWS_OMNIBOX_UI_FEATURE,
        FeatureConstants.TRANSLATE_MENU_BUTTON_FEATURE,
        FeatureConstants.CONTEXTUAL_SEARCH_TRANSLATION_ENABLE_FEATURE,
        FeatureConstants.CONTEXTUAL_SEARCH_WEB_SEARCH_FEATURE,
        FeatureConstants.CONTEXTUAL_SEARCH_PROMOTE_TAP_FEATURE,
        FeatureConstants.CONTEXTUAL_SEARCH_PROMOTE_PANEL_OPEN_FEATURE,
        FeatureConstants.CONTEXTUAL_SEARCH_OPT_IN_FEATURE,
        FeatureConstants.CONTEXTUAL_SEARCH_TAPPED_BUT_SHOULD_LONGPRESS_FEATURE,
        FeatureConstants.CONTEXTUAL_SEARCH_IN_PANEL_HELP_FEATURE,
        FeatureConstants.KEYBOARD_ACCESSORY_ADDRESS_FILL_FEATURE,
        FeatureConstants.KEYBOARD_ACCESSORY_BAR_SWIPING_FEATURE,
        FeatureConstants.KEYBOARD_ACCESSORY_PASSWORD_FILLING_FEATURE,
        FeatureConstants.KEYBOARD_ACCESSORY_PAYMENT_FILLING_FEATURE,
        FeatureConstants.KEYBOARD_ACCESSORY_PAYMENT_OFFER_FEATURE,
        FeatureConstants.DOWNLOAD_SETTINGS_FEATURE,
        FeatureConstants.DOWNLOAD_INFOBAR_DOWNLOAD_CONTINUING_FEATURE,
        FeatureConstants.DOWNLOAD_INFOBAR_DOWNLOADS_ARE_FASTER_FEATURE,
        FeatureConstants.NEW_TAB_PAGE_HOME_BUTTON_FEATURE,
        FeatureConstants.TAB_GROUPS_QUICKLY_COMPARE_PAGES_FEATURE,
        FeatureConstants.TAB_GROUPS_TAP_TO_SEE_ANOTHER_TAB_FEATURE,
        FeatureConstants.TAB_GROUPS_YOUR_TABS_ARE_TOGETHER_FEATURE,
        FeatureConstants.TAB_SWITCHER_BUTTON_FEATURE, FeatureConstants.FEED_CARD_MENU_FEATURE,
        FeatureConstants.IDENTITY_DISC_FEATURE, FeatureConstants.TAB_GROUPS_DRAG_AND_DROP_FEATURE,
        FeatureConstants.QUIET_NOTIFICATION_PROMPTS_FEATURE,
        FeatureConstants.FEED_HEADER_MENU_FEATURE,
        FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_1_FEATURE,
        FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_2_FEATURE,
        FeatureConstants.CHROME_REENGAGEMENT_NOTIFICATION_3_FEATURE,
        FeatureConstants.PWA_INSTALL_AVAILABLE_FEATURE, FeatureConstants.PAGE_INFO_FEATURE,
        FeatureConstants.IPH_SHARE_SCREENSHOT_FEATURE})
@Retention(RetentionPolicy.SOURCE)
public @interface FeatureConstants {
    String ADD_TO_HOMESCREEN_MESSAGE_FEATURE = "IPH_AddToHomescreenMessage";
    String ADD_TO_HOMESCREEN_TEXT_BUBBLE_FEATURE = "IPH_AddToHomescreenTextBubble";
    String DOWNLOAD_PAGE_FEATURE = "IPH_DownloadPage";
    String DOWNLOAD_PAGE_SCREENSHOT_FEATURE = "IPH_DownloadPageScreenshot";
    String DOWNLOAD_HOME_FEATURE = "IPH_DownloadHome";
    String DOWNLOAD_INDICATOR_FEATURE = "IPH_DownloadIndicator";
    String CHROME_HOME_EXPAND_FEATURE = "IPH_ChromeHomeExpand";
    String CHROME_HOME_PULL_TO_REFRESH_FEATURE = "IPH_ChromeHomePullToRefresh";
    String DATA_SAVER_PREVIEW_FEATURE = "IPH_DataSaverPreview";
    String DATA_SAVER_DETAIL_FEATURE = "IPH_DataSaverDetail";
    String DATA_SAVER_MILESTONE_PROMO_FEATURE = "IPH_DataSaverMilestonePromo";
    String EPHEMERAL_TAB_FEATURE = "IPH_EphemeralTab";
    String KEYBOARD_ACCESSORY_ADDRESS_FILL_FEATURE = "IPH_KeyboardAccessoryAddressFilling";
    String KEYBOARD_ACCESSORY_PASSWORD_FILLING_FEATURE = "IPH_KeyboardAccessoryPasswordFilling";
    String KEYBOARD_ACCESSORY_PAYMENT_FILLING_FEATURE = "IPH_KeyboardAccessoryPaymentFilling";
    String KEYBOARD_ACCESSORY_PAYMENT_OFFER_FEATURE = "IPH_KeyboardAccessoryPaymentOffer";
    String KEYBOARD_ACCESSORY_BAR_SWIPING_FEATURE = "IPH_KeyboardAccessoryBarSwiping";
    String PREVIEWS_OMNIBOX_UI_FEATURE = "IPH_PreviewsOmniboxUI";
    String TRANSLATE_MENU_BUTTON_FEATURE = "IPH_TranslateMenuButton";
    String EXPLORE_SITES_TILE_FEATURE = "IPH_ExploreSitesTile";
    String READ_LATER_CONTEXT_MENU_FEATURE = "IPH_ReadLaterContextMenu";
    String READ_LATER_APP_MENU_BOOKMARK_THIS_PAGE_FEATURE = "IPH_ReadLaterAppMenuBookmarkThisPage";
    String READ_LATER_APP_MENU_BOOKMARKS_FEATURE = "IPH_ReadLaterAppMenuBookmarks";
    String READ_LATER_BOTTOM_SHEET_FEATURE = "IPH_ReadLaterBottomSheet";

    /**
     * An IPH feature that encourages users to get better translations by enabling access to page
     * content.
     */
    String CONTEXTUAL_SEARCH_TRANSLATION_ENABLE_FEATURE = "IPH_ContextualSearchTranslationEnable";

    /**
     * An IPH feature that encourages users who search a query from a web page in a new tab, to use
     * Contextual Search instead.
     */
    String CONTEXTUAL_SEARCH_WEB_SEARCH_FEATURE = "IPH_ContextualSearchWebSearch";

    /**
     * An IPH feature for promoting tap over longpress for activating Contextual Search.
     */
    String CONTEXTUAL_SEARCH_PROMOTE_TAP_FEATURE = "IPH_ContextualSearchPromoteTap";

    /**
     * An IPH feature for encouraging users to open the Contextual Search Panel.
     */
    String CONTEXTUAL_SEARCH_PROMOTE_PANEL_OPEN_FEATURE = "IPH_ContextualSearchPromotePanelOpen";

    /**
     * An IPH feature for encouraging users to opt-in for Contextual Search.
     */
    String CONTEXTUAL_SEARCH_OPT_IN_FEATURE = "IPH_ContextualSearchOptIn";

    /**
     * An IPH feature educating users that tap to use longpress instead.
     */
    String CONTEXTUAL_SEARCH_TAPPED_BUT_SHOULD_LONGPRESS_FEATURE =
            "IPH_ContextualSearchTappedButShouldLongpress";

    /**
     * Another IPH to use longpress instead of tap, but this one appears inside the Panel.
     */
    String CONTEXTUAL_SEARCH_IN_PANEL_HELP_FEATURE = "IPH_ContextualSearchInPanelHelp";

    /**
     * An IPH feature indicating to users that there are settings for downloads and they are
     * accessible through Downloads Home.
     */
    String DOWNLOAD_SETTINGS_FEATURE = "IPH_DownloadSettings";

    /**
     * An IPH feature informing the users that even though infobar was closed, downloads are still
     * continuing in the background.
     */
    String DOWNLOAD_INFOBAR_DOWNLOAD_CONTINUING_FEATURE = "IPH_DownloadInfoBarDownloadContinuing";

    /**
     * An IPH feature that points to the download progress infobar and informs users that downloads
     * are now faster than before.
     */
    String DOWNLOAD_INFOBAR_DOWNLOADS_ARE_FASTER_FEATURE = "IPH_DownloadInfoBarDownloadsAreFaster";

    /**
     * An IPH feature attached to the mic button in the toolbar prompring user
     * to try voice.
     */
    String IPH_MIC_TOOLBAR_FEATURE = "IPH_MicToolbar";

    /** An IPH feature to prompt users to open the new tab page after a navigation. */
    String NEW_TAB_PAGE_HOME_BUTTON_FEATURE = "IPH_NewTabPageHomeButton";

    /**
     * An IPH feature to prompt the user to long press on pages with links to open them in a group.
     */
    String TAB_GROUPS_QUICKLY_COMPARE_PAGES_FEATURE = "IPH_TabGroupsQuicklyComparePages";

    /**
     * An IPH feature to show when the tabstrip shows to explain what each button does.
     */
    String TAB_GROUPS_TAP_TO_SEE_ANOTHER_TAB_FEATURE = "IPH_TabGroupsTapToSeeAnotherTab";

    /**
     * An IPH feature to show on tab switcher cards with multiple tab thumbnails.
     */
    String TAB_GROUPS_YOUR_TABS_ARE_TOGETHER_FEATURE = "IPH_TabGroupsYourTabsTogether";

    /** An IPH feature to prompt users to open the tab switcher after a navigation. */
    String TAB_SWITCHER_BUTTON_FEATURE = "IPH_TabSwitcherButton";

    /**
     * An IPH feature to show a card item on grid tab switcher to educate drag-and-drop.
     */
    String TAB_GROUPS_DRAG_AND_DROP_FEATURE = "IPH_TabGroupsDragAndDrop";

    /**
     * An IPH feature to show a video tutorial card on NTP to educate about an introduction to
     * chrome.
     */
    String VIDEO_TUTORIAL_NTP_CHROME_INTRO_FEATURE = "IPH_VideoTutorial_NTP_ChromeIntro";

    /**
     * An IPH feature to show a video tutorial card on NTP to educate about downloading in chrome.
     */
    String VIDEO_TUTORIAL_NTP_DOWNLOAD_FEATURE = "IPH_VideoTutorial_NTP_Download";

    /**
     * An IPH feature to show a video tutorial card on NTP to educate about how to search in chrome.
     */
    String VIDEO_TUTORIAL_NTP_SEARCH_FEATURE = "IPH_VideoTutorial_NTP_Search";

    /**
     * An IPH feature to show a video tutorial card on NTP to educate about how to use voice search
     * in chrome.
     */
    String VIDEO_TUTORIAL_NTP_VOICE_SEARCH_FEATURE = "IPH_VideoTutorial_NTP_VoiceSearch";

    /**
     * An IPH feature to show a video tutorial summary card on NTP that takes them to see the video
     * tutorial list page.
     */
    String VIDEO_TUTORIAL_NTP_SUMMARY_FEATURE = "IPH_VideoTutorial_NTP_Summary";

    /**
     * An IPH feature to show on a card menu on the FeedNewTabPage.
     */
    String FEED_CARD_MENU_FEATURE = "IPH_FeedCardMenu";

    /**
     * An IPH feature prompting user to tap on identity disc to navigate to "Sync and Google
     * services" preferences.
     */
    String IDENTITY_DISC_FEATURE = "IPH_IdentityDisc";

    /**
     * An IPH feature showing up the first time the user is presented with the quieter version of
     * the permission prompt (for notifications).
     */
    String QUIET_NOTIFICATION_PROMPTS_FEATURE = "IPH_QuietNotificationPrompts";

    /**
     * An IPH feature to show on the feed header menu button of the FeedNewTabPage.
     */
    String FEED_HEADER_MENU_FEATURE = "IPH_FeedHeaderMenu";

    /**
     * An IPH feature to show the first re-engagement notification.
     */
    String CHROME_REENGAGEMENT_NOTIFICATION_1_FEATURE = "IPH_ChromeReengagementNotification1";

    /**
     * An IPH feature to show the second re-engagement notification.
     */
    String CHROME_REENGAGEMENT_NOTIFICATION_2_FEATURE = "IPH_ChromeReengagementNotification2";

    /**
     * An IPH feature to show the third re-engagement notification.
     */
    String CHROME_REENGAGEMENT_NOTIFICATION_3_FEATURE = "IPH_ChromeReengagementNotification3";

    /**
     * An IPH feature to inform users that installing a PWA is an option.
     */
    String PWA_INSTALL_AVAILABLE_FEATURE = "IPH_PwaInstallAvailable";

    /**
     * An IPH feature to inform about changing permissions in PageInfo.
     */
    String PAGE_INFO_FEATURE = "IPH_PageInfo";

    /**
     * An IPH feature to inform users about the screenshot sharing feature.
     */
    String IPH_SHARE_SCREENSHOT_FEATURE = "IPH_ShareScreenshot";
}
