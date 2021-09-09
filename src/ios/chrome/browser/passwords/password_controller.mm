// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/password_controller.h"

#include <stddef.h>

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/autofill/core/common/password_form_generation_data.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/autofill/ios/browser/autofill_util.h"
#import "components/autofill/ios/form_util/form_activity_observer_bridge.h"
#include "components/autofill/ios/form_util/form_activity_params.h"
#include "components/autofill/ios/form_util/unique_id_data_tab_helper.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_generation_frame_helper.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/ios/account_select_fill_data.h"
#import "components/password_manager/ios/password_form_helper.h"
#import "components/password_manager/ios/password_suggestion_helper.h"
#import "components/password_manager/ios/shared_password_controller.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/driver/sync_service.h"
#import "components/ukm/ios/ukm_url_recorder.h"
#import "ios/chrome/browser/autofill/form_input_accessory_view_handler.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/infobars/infobar_ios.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/infobar_type.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/passwords/ios_chrome_save_password_infobar_delegate.h"
#import "ios/chrome/browser/passwords/notify_auto_signin_view_controller.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"
#import "ios/chrome/browser/ui/alert_coordinator/action_sheet_coordinator.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/password_breach_commands.h"
#import "ios/chrome/browser/ui/commands/password_protection_commands.h"
#import "ios/chrome/browser/ui/infobars/coordinators/infobar_password_coordinator.h"
#import "ios/chrome/browser/ui/infobars/infobar_feature.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#include "ios/chrome/browser/web/tab_id_tab_helper.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/web/common/url_scheme_util.h"
#include "ios/web/public/js_messaging/web_frame.h"
#include "ios/web/public/js_messaging/web_frame_util.h"
#include "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using autofill::FormActivityObserverBridge;
using autofill::FormData;
using autofill::PasswordFormGenerationData;
using password_manager::PasswordForm;
using autofill::FormRendererId;
using autofill::FieldRendererId;
using base::SysNSStringToUTF16;
using base::SysUTF16ToNSString;
using base::SysUTF8ToNSString;
using l10n_util::GetNSString;
using l10n_util::GetNSStringF;
using password_manager::metrics_util::LogPasswordDropdownShown;
using password_manager::metrics_util::PasswordDropdownState;
using password_manager::AccountSelectFillData;
using password_manager::FillData;
using password_manager::GetPageURLAndCheckTrustLevel;
using password_manager::PasswordFormManagerForUI;
using password_manager::PasswordGenerationFrameHelper;
using password_manager::PasswordManager;
using password_manager::PasswordManagerClient;
using password_manager::PasswordManagerDriver;
using web::WebFrame;
using web::WebState;

namespace {
// Types of password infobars to display.
enum class PasswordInfoBarType { SAVE, UPDATE };

// Duration for notify user auto-sign in dialog being displayed.
constexpr int kNotifyAutoSigninDuration = 3;  // seconds
}  // namespace

@interface PasswordController () <SharedPasswordControllerDelegate>

// View controller for auto sign-in notification, owned by this
// PasswordController.
@property(nonatomic, strong)
    NotifyUserAutoSigninViewController* notifyAutoSigninViewController;

// The action sheet coordinator, if one is currently being shown.
@property(nonatomic, strong) ActionSheetCoordinator* actionSheetCoordinator;

// Tracks current potential generated password until accepted or rejected.
@property(nonatomic, copy) NSString* generatedPotentialPassword;

// Displays infobar for |form| with |type|. If |type| is UPDATE, the user
// is prompted to update the password. If |type| is SAVE, the user is prompted
// to save the password.
- (void)showInfoBarForForm:(std::unique_ptr<PasswordFormManagerForUI>)form
               infoBarType:(PasswordInfoBarType)type
                    manual:(BOOL)manual;

// Removes infobar for given |type| if it exists. If it is not found the
// request is silently ignored (because that use case is expected).
- (void)removeInfoBarOfType:(PasswordInfoBarType)type manual:(BOOL)manual;

// Hides auto sign-in notification. Removes the view from superview and destroys
// the controller.
// TODO(crbug.com/435048): Animate disappearance.
- (void)hideAutosigninNotification;

@end

@implementation PasswordController {
  std::unique_ptr<PasswordManager> _passwordManager;
  std::unique_ptr<PasswordManagerClient> _passwordManagerClient;
  std::unique_ptr<PasswordManagerDriver> _passwordManagerDriver;

  // The WebState this instance is observing. Will be null after
  // -webStateDestroyed: has been called.
  WebState* _webState;

  // Bridge to observe WebState from Objective-C.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;

  // Timer for hiding "Signing in as ..." notification.
  base::OneShotTimer _notifyAutoSigninTimer;

  // User credential waiting to be displayed in autosign-in snackbar, once tab
  // becomes active.
  std::unique_ptr<PasswordForm> _pendingAutoSigninPasswordForm;
}

- (instancetype)initWithWebState:(WebState*)webState {
  self = [self initWithWebState:webState client:nullptr];
  return self;
}

- (instancetype)initWithWebState:(WebState*)webState
                          client:(std::unique_ptr<PasswordManagerClient>)
                                     passwordManagerClient {
  self = [super init];
  if (self) {
    DCHECK(webState);
    _webState = webState;
    _webStateObserverBridge =
        std::make_unique<web::WebStateObserverBridge>(self);
    _webState->AddObserver(_webStateObserverBridge.get());
    if (passwordManagerClient) {
      _passwordManagerClient = std::move(passwordManagerClient);
    } else {
      _passwordManagerClient.reset(new IOSChromePasswordManagerClient(self));
    }
    _passwordManager.reset(new PasswordManager(_passwordManagerClient.get()));

    PasswordFormHelper* formHelper =
        [[PasswordFormHelper alloc] initWithWebState:webState];
    PasswordSuggestionHelper* suggestionHelper =
        [[PasswordSuggestionHelper alloc] init];
    _sharedPasswordController = [[SharedPasswordController alloc]
        initWithWebState:_webState
                 manager:_passwordManager.get()
              formHelper:formHelper
        suggestionHelper:suggestionHelper];
    _sharedPasswordController.delegate = self;
    _passwordManagerDriver.reset(new IOSChromePasswordManagerDriver(
        _sharedPasswordController, _passwordManager.get()));
  }
  return self;
}

- (void)dealloc {
  if (_webState) {
    _webState->RemoveObserver(_webStateObserverBridge.get());
  }
}

#pragma mark - Properties

- (ukm::SourceId)ukmSourceId {
  return _webState ? ukm::GetSourceIdForWebStateDocument(_webState)
                   : ukm::kInvalidSourceId;
}

- (PasswordManagerClient*)passwordManagerClient {
  return _passwordManagerClient.get();
}

- (PasswordManagerDriver*)passwordManagerDriver {
  return _passwordManagerDriver.get();
}

#pragma mark - CRWWebStateObserver

// If Tab was shown, and there is a pending PasswordForm, display autosign-in
// notification.
- (void)webStateWasShown:(WebState*)webState {
  DCHECK_EQ(_webState, webState);
  if (_pendingAutoSigninPasswordForm) {
    [self showAutosigninNotification:std::move(_pendingAutoSigninPasswordForm)];
    _pendingAutoSigninPasswordForm.reset();
  }
}

// If Tab was hidden, hide auto sign-in notification.
- (void)webStateWasHidden:(WebState*)webState {
  DCHECK_EQ(_webState, webState);
  [self hideAutosigninNotification];
}

- (void)webStateDestroyed:(WebState*)webState {
  DCHECK_EQ(_webState, webState);
  if (_webState) {
    _webState->RemoveObserver(_webStateObserverBridge.get());
    _webStateObserverBridge.reset();
    _webState = nullptr;
  }
  _passwordManagerDriver.reset();
  _passwordManager.reset();
  _passwordManagerClient.reset();
}

#pragma mark - FormSuggestionProvider

- (id<FormSuggestionProvider>)suggestionProvider {
  return _sharedPasswordController;
}

#pragma mark - PasswordGenerationProvider

- (id<PasswordGenerationProvider>)generationProvider {
  return _sharedPasswordController;
}

#pragma mark - IOSChromePasswordManagerClientBridge

- (WebState*)webState {
  return _webState;
}

- (ChromeBrowserState*)browserState {
  return _webState ? ChromeBrowserState::FromBrowserState(
                         _webState->GetBrowserState())
                   : nullptr;
}

- (PasswordManager*)passwordManager {
  return _passwordManager.get();
}

- (const GURL&)lastCommittedURL {
  return _webState ? _webState->GetLastCommittedURL() : GURL::EmptyGURL();
}

- (void)showSavePasswordInfoBar:
            (std::unique_ptr<PasswordFormManagerForUI>)formToSave
                         manual:(BOOL)manual {
  [self showInfoBarForForm:std::move(formToSave)
               infoBarType:PasswordInfoBarType::SAVE
                    manual:manual];
}

- (void)showUpdatePasswordInfoBar:
            (std::unique_ptr<PasswordFormManagerForUI>)formToUpdate
                           manual:(BOOL)manual {
  [self showInfoBarForForm:std::move(formToUpdate)
               infoBarType:PasswordInfoBarType::UPDATE
                    manual:manual];
}

- (void)removePasswordInfoBarManualFallback:(BOOL)manual {
  [self removeInfoBarOfType:PasswordInfoBarType::SAVE manual:manual];
  [self removeInfoBarOfType:PasswordInfoBarType::UPDATE manual:manual];
}

// Shows auto sign-in notification and schedules hiding it after 3 seconds.
// TODO(crbug.com/435048): Animate appearance.
- (void)showAutosigninNotification:(std::unique_ptr<PasswordForm>)formSignedIn {
  if (!_webState) {
    return;
  }

  // If a notification is already being displayed, hides the old one, then shows
  // the new one.
  if (self.notifyAutoSigninViewController) {
    _notifyAutoSigninTimer.Stop();
    [self hideAutosigninNotification];
  }

  // Creates view controller then shows the subview.
  self.notifyAutoSigninViewController =
      [[NotifyUserAutoSigninViewController alloc]
          initWithUsername:SysUTF16ToNSString(formSignedIn->username_value)
                   iconURL:formSignedIn->icon_url
          URLLoaderFactory:_webState->GetBrowserState()
                               ->GetSharedURLLoaderFactory()];
  TabIdTabHelper* tabIdHelper = TabIdTabHelper::FromWebState(_webState);
  if (![_delegate displaySignInNotification:self.notifyAutoSigninViewController
                                  fromTabId:tabIdHelper->tab_id()]) {
    // The notification was not shown. Store the password form in
    // |_pendingAutoSigninPasswordForm| to show the notification later.
    _pendingAutoSigninPasswordForm = std::move(formSignedIn);
    self.notifyAutoSigninViewController = nil;
    return;
  }

  // Hides notification after 3 seconds.
  __weak PasswordController* weakSelf = self;
  _notifyAutoSigninTimer.Start(
      FROM_HERE, base::TimeDelta::FromSeconds(kNotifyAutoSigninDuration),
      base::BindRepeating(^{
        [weakSelf hideAutosigninNotification];
      }));
}

- (void)showPasswordBreachForLeakType:(CredentialLeakType)leakType
                                  URL:(const GURL&)URL {
  [self.passwordBreachDispatcher showPasswordBreachForLeakType:leakType];
}

- (void)showPasswordProtectionWarning:(NSString*)warningText
                           completion:(void (^)(safe_browsing::WarningAction))
                                          completion {
  [self.passwordProtectionDispatcher showPasswordProtectionWarning:warningText
                                                        completion:completion];
}

#pragma mark - Private methods

// The dispatcher used for ApplicationCommands.
- (id<ApplicationCommands>)applicationCommandsHandler {
  DCHECK(self.dispatcher);
  return HandlerForProtocol(self.dispatcher, ApplicationCommands);
}

// The dispatcher used for PasswordBreachCommands.
- (id<PasswordBreachCommands>)passwordBreachDispatcher {
  DCHECK(self.dispatcher);
  return HandlerForProtocol(self.dispatcher, PasswordBreachCommands);
}

// The dispatcher used for PasswordProtectionCommands.
- (id<PasswordProtectionCommands>)passwordProtectionDispatcher {
  DCHECK(self.dispatcher);
  return HandlerForProtocol(self.dispatcher, PasswordProtectionCommands);
}

- (InfoBarIOS*)findInfobarOfType:(InfobarType)infobarType manual:(BOOL)manual {
  infobars::InfoBarManager* infoBarManager =
      InfoBarManagerImpl::FromWebState(_webState);

  size_t count = infoBarManager->infobar_count();
  for (size_t i = 0; i < count; i++) {
    InfoBarIOS* infobar =
        static_cast<InfoBarIOS*>(infoBarManager->infobar_at(i));
    if (infobar->infobar_type() == infobarType &&
        infobar->skip_banner() == manual)
      return infobar;
  }

  return nil;
}

- (void)removeInfoBarOfType:(PasswordInfoBarType)type manual:(BOOL)manual {

  InfoBarIOS* infobar = nil;
  switch (type) {
    case PasswordInfoBarType::SAVE: {
      infobar = [self findInfobarOfType:InfobarType::kInfobarTypePasswordSave
                                 manual:manual];
      break;
    }
    case PasswordInfoBarType::UPDATE: {
      infobar = [self findInfobarOfType:InfobarType::kInfobarTypePasswordUpdate
                                 manual:manual];
      break;
    }
  }

  if (infobar) {
    InfoBarManagerImpl::FromWebState(_webState)->RemoveInfoBar(infobar);
  }
}

- (void)showInfoBarForForm:(std::unique_ptr<PasswordFormManagerForUI>)form
               infoBarType:(PasswordInfoBarType)type
                    manual:(BOOL)manual {
  if (!_webState) {
    return;
  }

  bool isSyncUser = false;
  if (self.browserState) {
    syncer::SyncService* syncService =
        ProfileSyncServiceFactory::GetForBrowserState(self.browserState);
    isSyncUser = password_bubble_experiment::IsSmartLockUser(syncService);
  }
  infobars::InfoBarManager* infoBarManager =
      InfoBarManagerImpl::FromWebState(_webState);

  switch (type) {
    case PasswordInfoBarType::SAVE: {
      auto delegate = std::make_unique<IOSChromeSavePasswordInfoBarDelegate>(
          isSyncUser, /*password_update*/ false, std::move(form));
      delegate->set_handler(self.applicationCommandsHandler);

        // Count only new infobar showings, not replacements.
        if (![self findInfobarOfType:InfobarType::kInfobarTypePasswordSave
                              manual:manual]) {
          base::UmaHistogramBoolean("PasswordManager.iOS.InfoBar.PasswordSave",
                                    true);
        }

        std::unique_ptr<InfoBarIOS> infobar;

        // If manual save, skip showing banner.
        bool skipBanner = manual;
        if (IsInfobarOverlayUIEnabled()) {
          infobar = std::make_unique<InfoBarIOS>(
              InfobarType::kInfobarTypePasswordSave, std::move(delegate),
              skipBanner);
        } else {
          InfobarPasswordCoordinator* coordinator = [[InfobarPasswordCoordinator
              alloc]
              initWithInfoBarDelegate:delegate.get()
                                 type:InfobarType::kInfobarTypePasswordSave];
          infobar = std::make_unique<InfoBarIOS>(
              coordinator, std::move(delegate), skipBanner);
        }
        infoBarManager->AddInfoBar(std::move(infobar),
                                   /*replace_existing=*/true);
      break;
    }
    case PasswordInfoBarType::UPDATE: {
        // Count only new infobar showings, not replacements.
        if (![self findInfobarOfType:InfobarType::kInfobarTypePasswordUpdate
                              manual:manual]) {
          base::UmaHistogramBoolean(
              "PasswordManager.iOS.InfoBar.PasswordUpdate", true);
        }

        auto delegate = std::make_unique<IOSChromeSavePasswordInfoBarDelegate>(
            isSyncUser, /*password_update*/ true, std::move(form));
        delegate->set_handler(self.applicationCommandsHandler);
        std::unique_ptr<InfoBarIOS> infobar;
        // If manual save, skip showing banner.
        if (IsInfobarOverlayUIEnabled()) {
          infobar = std::make_unique<InfoBarIOS>(
              InfobarType::kInfobarTypePasswordUpdate, std::move(delegate),
              /*=skip_banner*/ manual);
        } else {
          InfobarPasswordCoordinator* coordinator = [[InfobarPasswordCoordinator
              alloc]
              initWithInfoBarDelegate:delegate.get()
                                 type:InfobarType::kInfobarTypePasswordUpdate];
          infobar = std::make_unique<InfoBarIOS>(
              coordinator, std::move(delegate), /*skip_banner=*/manual);
        }
        infoBarManager->AddInfoBar(std::move(infobar),
                                   /*replace_existing=*/true);
      break;
    }
  }
}

- (void)hideAutosigninNotification {
  [self.notifyAutoSigninViewController willMoveToParentViewController:nil];
  [self.notifyAutoSigninViewController.view removeFromSuperview];
  [self.notifyAutoSigninViewController removeFromParentViewController];
  self.notifyAutoSigninViewController = nil;
}

- (void)generatePasswordPopupDismissed {
  [self.actionSheetCoordinator stop];
  self.actionSheetCoordinator = nil;
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  self.generatedPotentialPassword = nil;
}

- (void)updateGeneratePasswordStrings:(id)sender {
  NSString* title = [NSString
      stringWithFormat:@"%@\n%@\n ", GetNSString(IDS_IOS_SUGGESTED_PASSWORD),
                       self.generatedPotentialPassword];
  self.actionSheetCoordinator.attributedTitle =
      [[NSMutableAttributedString alloc]
          initWithString:title
              attributes:@{
                NSFontAttributeName :
                    [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline]
              }];

  NSString* message = GetNSString(IDS_IOS_SUGGESTED_PASSWORD_HINT);
  self.actionSheetCoordinator.attributedMessage =
      [[NSMutableAttributedString alloc]
          initWithString:message
              attributes:@{
                NSFontAttributeName :
                    [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote]
              }];

  // TODO(crbug.com/886583): find a way to make action sheet coordinator
  // responsible for font size changes.
  [self.actionSheetCoordinator updateAttributedText];
}

#pragma mark - SharedPasswordControllerDelegate

- (void)sharedPasswordController:(SharedPasswordController*)controller
    showGeneratedPotentialPassword:(NSString*)generatedPotentialPassword
                   decisionHandler:(void (^)(BOOL accept))decisionHandler {
  self.generatedPotentialPassword = generatedPotentialPassword;

  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(updateGeneratePasswordStrings:)
             name:UIContentSizeCategoryDidChangeNotification
           object:nil];

  // TODO(crbug.com/886583): add eg tests
  self.actionSheetCoordinator = [[ActionSheetCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:nullptr
                           title:@""
                         message:@""
                            rect:self.baseViewController.view.frame
                            view:self.baseViewController.view];
  self.actionSheetCoordinator.popoverArrowDirection = 0;
  self.actionSheetCoordinator.alertStyle =
      IsIPadIdiom() ? UIAlertControllerStyleAlert
                    : UIAlertControllerStyleActionSheet;

  // Set attributed text.
  [self updateGeneratePasswordStrings:self];

  __weak PasswordController* weakSelf = self;

  auto popupDismissed = ^{
    [weakSelf generatePasswordPopupDismissed];
  };

  auto closeKeyboard = ^{
    if (!weakSelf.webState) {
      return;
    }
    FormInputAccessoryViewHandler* handler =
        [[FormInputAccessoryViewHandler alloc] init];
    NSString* mainFrameID =
        SysUTF8ToNSString(web::GetMainWebFrameId(weakSelf.webState));
    [handler setLastFocusFormActivityWebFrameID:mainFrameID];
    [handler closeKeyboardWithoutButtonPress];
  };

  [self.actionSheetCoordinator
      addItemWithTitle:GetNSString(IDS_IOS_USE_SUGGESTED_PASSWORD)
                action:^{
                  decisionHandler(YES);
                  popupDismissed();
                  closeKeyboard();
                }
                 style:UIAlertActionStyleDefault];

  [self.actionSheetCoordinator addItemWithTitle:GetNSString(IDS_CANCEL)
                                         action:^{
                                           decisionHandler(NO);
                                           popupDismissed();
                                         }
                                          style:UIAlertActionStyleCancel];

  // Set 'suggest' as preferred action, as per UX.
  self.actionSheetCoordinator.alertController.preferredAction =
      self.actionSheetCoordinator.alertController.actions[0];

  [self.actionSheetCoordinator start];
}

- (void)sharedPasswordController:(SharedPasswordController*)controller
             didAcceptSuggestion:(FormSuggestion*)suggestion {
  if (suggestion.identifier ==
      autofill::POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY) {
    // Navigate to the settings list.
    [self.delegate displaySavedPasswordList];
  }
}

@end
