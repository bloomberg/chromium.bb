// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_DEFAULT_PROMO_DEFAULT_BROWSER_UTILS_H_
#define IOS_CHROME_BROWSER_UI_DEFAULT_PROMO_DEFAULT_BROWSER_UTILS_H_

#import <UIKit/UIKit.h>

// Enum for the different types of default browser modal promo. These are stored
// as values, if adding a new one, make sure to add it at the end.
typedef NS_ENUM(NSUInteger, DefaultPromoType) {
  DefaultPromoTypeGeneral = 0,
  DefaultPromoTypeStaySafe = 1,
  DefaultPromoTypeMadeForIOS = 2,
  DefaultPromoTypeAllTabs = 3
};

namespace {

// Enum actions for the IOS.DefaultBrowserFullscreenPromo* UMA metrics. Entries
// should not be renumbered and numeric values should never be reused.
enum IOSDefaultBrowserFullscreenPromoAction {
  ACTION_BUTTON = 0,
  CANCEL = 1,
  REMIND_ME_LATER = 2,
  kMaxValue = REMIND_ME_LATER,
};

}  // namespace

// UserDefaults key that saves the last time an HTTP(S) link was sent and opened
// by the app.
extern NSString* const kLastHTTPURLOpenTime;

// The feature parameter to activate the remind me later button.
extern const char kDefaultBrowserFullscreenPromoExperimentRemindMeGroupParam[];

// Logs the timestamp of user activity that is deemed to be an indication of
// a user that would likely benefit from having Chrome set as their default
// browser. Before logging the current activity, this method will also clear all
// past expired logs for |type| that have happened too far in the past.
void LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoType type);

// Logs the timestamp of a user tap on the "Remind Me Later" button in the
// Fullscreen Promo.
void LogRemindMeLaterPromoActionInteraction();

// Returns true if the user has tapped on the "Remind Me Later" button and the
// delay time threshold has been met.
bool ShouldShowRemindMeLaterDefaultBrowserFullscreenPromo();

// Returns true if the user is in the group that will be shown the Remind Me
// Later button in the fullscreen promo.
bool IsInRemindMeLaterGroup();

// Returns true if the user is in the group that will be shown a modified
// description and "Learn More" text.
bool IsInModifiedStringsGroup();

// Returns true if the user is in the CTA experiment in the open links group.
bool IsInCTAOpenLinksGroup();

// Returns true if the user is in the CTA experiment in the switch group.
bool IsInCTASwitchGroup();

// Returns true if non modals default browser promos are enabled.
bool NonModalPromosEnabled();

// Returns true if the user has interacted with the Fullscreen Promo previously.
// Returns false otherwise.
bool HasUserInteractedWithFullscreenPromoBefore();

// Returns true if the user has interacted with a tailored Fullscreen Promo
// previously. Returns false otherwise.
bool HasUserInteractedWithTailoredFullscreenPromoBefore();

// Returns YES if the user taps on open settings button from first run promo.
BOOL HasUserOpenedSettingsFromFirstRunPromo();

// Returns the number of times the user has seen and interacted with the
// non-modal promo before.
int UserInteractionWithNonModalPromoCount();

// Logs that the user has interacted with the Fullscreen Promo.
void LogUserInteractionWithFullscreenPromo();

// Logs that the user has interacted with a Tailored Fullscreen Promo.
void LogUserInteractionWithTailoredFullscreenPromo();

// Logs that the user has interacted with a Non-Modals Promo.
void LogUserInteractionWithNonModalPromo();

// Logs that the user has interacted with the first run promo.
void LogUserInteractionWithFirstRunPromo(BOOL openedSettings);

// Returns true if the last URL open is within the time threshold that would
// indicate Chrome is likely still the default browser. Returns false otherwise.
bool IsChromeLikelyDefaultBrowser();

// Do not use. Only for backward compatibility
// Returns true if the last URL open is within 7 days. Returns false otherwise.
bool IsChromeLikelyDefaultBrowser7Days();

// Returns true if the past behavior of the user indicates that the user fits
// the categorization that would likely benefit from having Chrome set as their
// default browser for the passed |type|. Returns false otherwise.
bool IsLikelyInterestedDefaultBrowserUser(DefaultPromoType type);

// Returns the most recent promo the user showed interest in. Defaults to
// DefaultPromoTypeGeneral if no interest is found. If |skipAllTabsPromo| is
// true, this type of promo will be ignored.
DefaultPromoType MostRecentInterestDefaultPromoType(BOOL skipAllTabsPromo);

// Return YES if the user has seen a promo recently, and shouldn't
// see another one.
BOOL UserInPromoCooldown();

#endif  // IOS_CHROME_BROWSER_UI_DEFAULT_PROMO_DEFAULT_BROWSER_UTILS_H_
