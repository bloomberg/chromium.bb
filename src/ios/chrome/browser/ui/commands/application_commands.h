// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COMMANDS_APPLICATION_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_COMMANDS_APPLICATION_COMMANDS_H_

#import <Foundation/Foundation.h>

#include "ios/public/provider/chrome/browser/user_feedback/user_feedback_sender.h"

@class OpenNewTabCommand;
@class ShowSigninCommand;
@class StartVoiceSearchCommand;
@class UIViewController;
namespace syncer {
enum class KeyRetrievalTriggerForUMA;
}  // namespace syncer

// This protocol groups commands that are part of ApplicationCommands, but
// may also be forwarded directly to a settings navigation controller.
@protocol ApplicationSettingsCommands

// TODO(crbug.com/779791) : Do not pass baseViewController through dispatcher.
// Shows the accounts settings UI, presenting from |baseViewController|. If
// |baseViewController| is nil BVC will be used as presenterViewController.
- (void)showAccountsSettingsFromViewController:
    (UIViewController*)baseViewController;

// TODO(crbug.com/779791) : Do not pass baseViewController through dispatcher.
// Shows the Google services settings UI, presenting from |baseViewController|.
// If |baseViewController| is nil BVC will be used as presenterViewController.
- (void)showGoogleServicesSettingsFromViewController:
    (UIViewController*)baseViewController;

// TODO(crbug.com/779791) : Do not pass baseViewController through dispatcher.
// Shows the Sync settings UI, presenting from |baseViewController|.
// If |baseViewController| is nil BVC will be used as presenterViewController.
- (void)showSyncSettingsFromViewController:
    (UIViewController*)baseViewController;

// TODO(crbug.com/779791) : Do not pass baseViewController through dispatcher.
// Shows the sync encryption passphrase UI, presenting from
// |baseViewController|.
- (void)showSyncPassphraseSettingsFromViewController:
    (UIViewController*)baseViewController;

// Shows the list of saved passwords in the settings.
- (void)showSavedPasswordsSettingsFromViewController:
    (UIViewController*)baseViewController;

// Shows the list of saved passwords in the settings. Automatically starts
// password check.
- (void)showSavedPasswordsSettingsAndStartPasswordCheckFromViewController:
    (UIViewController*)baseViewController;

// Shows the list of profiles (addresess) in the settings.
- (void)showProfileSettingsFromViewController:
    (UIViewController*)baseViewController;

// Shows the list of credit cards in the settings.
- (void)showCreditCardSettingsFromViewController:
    (UIViewController*)baseViewController;

@end

// Protocol for commands that will generally be handled by the application,
// rather than a specific tab; in practice this means the MainController
// instance.
// This protocol includes all of the methods in ApplicationSettingsCommands; an
// object that implements the methods in this protocol should be able to forward
// ApplicationSettingsCommands to the settings view controller if necessary.

@protocol ApplicationCommands <NSObject, ApplicationSettingsCommands>

// Dismisses all modal dialogs.
- (void)dismissModalDialogs;

// TODO(crbug.com/779791) : Do not pass baseViewController through dispatcher.
// Shows the Settings UI, presenting from |baseViewController|.
- (void)showSettingsFromViewController:(UIViewController*)baseViewController;

// TODO(crbug.com/779791) : Do not pass baseViewController through dispatcher.
// Shows the advanced sign-in settings.
- (void)showAdvancedSigninSettingsFromViewController:
    (UIViewController*)baseViewController;

- (void)showLocationPermissionsFromViewController:
    (UIViewController*)baseViewController;

// Presents the Trusted Vault reauth dialog.
// |baseViewController| presents the sign-in.
// |retrievalTrigger| UI elements where the trusted vault reauth has been
// triggered.
- (void)
    showTrustedVaultReauthenticationFromViewController:
        (UIViewController*)baseViewController
                                      retrievalTrigger:
                                          (syncer::KeyRetrievalTriggerForUMA)
                                              retrievalTrigger;

// Starts a voice search on the current BVC.
- (void)startVoiceSearch;

// Shows the History UI.
- (void)showHistory;

// Closes the History UI and opens a URL.
- (void)closeSettingsUIAndOpenURL:(OpenNewTabCommand*)command;

// Closes the History UI.
- (void)closeSettingsUI;

// Prepare to show the TabSwitcher UI.
- (void)prepareTabSwitcher;

// Shows the TabSwitcher UI. When the thumb strip is enabled, shows the
// TabSwitcher UI, specifically in its grid layout.
- (void)displayTabSwitcherInGridLayout;

// Same as displayTabSwitcherInGridLayout, but also force tab switcher to
// regular tabs page.
- (void)displayRegularTabSwitcherInGridLayout;

// TODO(crbug.com/779791) : Do not pass baseViewController through dispatcher.
// Shows the Autofill Settings UI, presenting from |baseViewController|.
- (void)showAutofillSettingsFromViewController:
    (UIViewController*)baseViewController;

// Shows the Report an Issue UI, presenting from |baseViewController|.
- (void)showReportAnIssueFromViewController:
            (UIViewController*)baseViewController
                                     sender:(UserFeedbackSender)sender;

// Shows the Report an Issue UI, presenting from |baseViewController|, using
// |specificProductData| for additional product data to be sent in the report.
- (void)
    showReportAnIssueFromViewController:(UIViewController*)baseViewController
                                 sender:(UserFeedbackSender)sender
                    specificProductData:(NSDictionary<NSString*, NSString*>*)
                                            specificProductData;

// Opens the |command| URL in a new tab.
// TODO(crbug.com/907527): Check if it is possible to merge it with the
// URLLoader methods.
- (void)openURLInNewTab:(OpenNewTabCommand*)command;

// TODO(crbug.com/779791) : Do not pass baseViewController through dispatcher.
// Shows the signin UI, presenting from |baseViewController|.
- (void)showSignin:(ShowSigninCommand*)command
    baseViewController:(UIViewController*)baseViewController;

// Signs the user out and dismisses UI for any in-progress sign-in.
- (void)forceSignOut;

// TODO(crbug.com/779791) : Do not pass baseViewController through dispatcher.
// Shows the consistency promo UI that allows users to sign in to Chrome using
// the default accounts on the device.
- (void)showConsistencyPromoFromViewController:
    (UIViewController*)baseViewController;

// Shows a notification with the signed-in user account.
- (void)showSigninAccountNotificationFromViewController:
    (UIViewController*)baseViewController;

// Sets whether the UI is displaying incognito content.
- (void)setIncognitoContentVisible:(BOOL)incognitoContentVisible;

// Open a new window with |userActivity|
- (void)openNewWindowWithActivity:(NSUserActivity*)userActivity;

@end

#endif  // IOS_CHROME_BROWSER_UI_COMMANDS_APPLICATION_COMMANDS_H_
