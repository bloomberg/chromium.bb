// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PREF_NAMES_H_
#define IOS_CHROME_BROWSER_PREF_NAMES_H_

namespace prefs {

extern const char kApplicationLocale[];
extern const char kArticlesForYouEnabled[];
extern const char kBrowserStateInfoCache[];
extern const char kBrowserStateLastUsed[];
extern const char kBrowserStatesLastActive[];
extern const char kBrowserStatesNumCreated[];
extern const char kBrowsingDataMigrationHasBeenPossible[];
extern const char kClearBrowsingDataHistoryNoticeShownTimes[];
extern const char kContextualSearchEnabled[];
extern const char kDataSaverEnabled[];
extern const char kDefaultCharset[];
extern const char kEnableDoNotTrack[];
extern const char kHttpServerProperties[];
extern const char kIncognitoModeAvailability[];
extern const char kIosBookmarkCachedFolderId[];
extern const char kIosBookmarkCachedTopMostRow[];
extern const char kIosBookmarkFolderDefault[];
extern const char kIosBookmarkPromoAlreadySeen[];
extern const char kIosBookmarkSigninPromoDisplayedCount[];
extern const char kIosDiscoverFeedLastRefreshTime[];
extern const char kIosSettingsPromoAlreadySeen[];
extern const char kIosSettingsSigninPromoDisplayedCount[];
extern const char kLinkPreviewEnabled[];
extern const char kNTPContentSuggestionsEnabled[];
extern const char kPrintingEnabled[];
extern const char kSearchSuggestEnabled[];
extern const char kTrackPricesOnTabsEnabled[];

extern const char kNetworkPredictionSetting[];

extern const char kNtpShownBookmarksFolder[];
extern const char kShowMemoryDebuggingTools[];

extern const char kSigninLastAccounts[];
extern const char kSigninLastAccountsMigrated[];
extern const char kSigninShouldPromptForSigninAgain[];
extern const char kSigninWebSignDismissalCount[];

extern const char kIosUserZoomMultipliers[];

extern const char kIncognitoAuthenticationSetting[];

extern const char kBrowserSigninPolicy[];

}  // namespace prefs

#endif  // IOS_CHROME_BROWSER_PREF_NAMES_H_
