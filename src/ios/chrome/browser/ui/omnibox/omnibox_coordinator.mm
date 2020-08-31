// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_coordinator.h"

#include "base/check.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/common/omnibox_focus_state.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/load_query_commands.h"
#import "ios/chrome/browser/ui/commands/omnibox_commands.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_constants.h"
#import "ios/chrome/browser/ui/omnibox/keyboard_assist/omnibox_assistive_keyboard_delegate.h"
#import "ios/chrome/browser/ui/omnibox/keyboard_assist/omnibox_assistive_keyboard_views.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_mediator.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_ios.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_util.h"
#include "ios/chrome/browser/ui/omnibox/omnibox_view_controller.h"
#include "ios/chrome/browser/ui/omnibox/omnibox_view_ios.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_coordinator.h"
#include "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_view_ios.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface OmniboxCoordinator () <OmniboxViewControllerDelegate>
// Object taking care of adding the accessory views to the keyboard.
@property(nonatomic, strong)
    OmniboxAssistiveKeyboardDelegateImpl* keyboardDelegate;

// View controller managed by this coordinator.
@property(nonatomic, strong) OmniboxViewController* viewController;

// The mediator for the omnibox.
@property(nonatomic, strong) OmniboxMediator* mediator;

@end

@implementation OmniboxCoordinator {
  // TODO(crbug.com/818636): use a slimmer subclass of OmniboxView,
  // OmniboxPopupViewSuggestionsDelegate instead of OmniboxViewIOS.
  std::unique_ptr<OmniboxViewIOS> _editView;
}
@synthesize editController = _editController;
@synthesize keyboardDelegate = _keyboardDelegate;
@synthesize viewController = _viewController;
@synthesize mediator = _mediator;

#pragma mark - public

- (void)start {
  BOOL isIncognito = self.browser->GetBrowserState()->IsOffTheRecord();

  self.viewController =
      [[OmniboxViewController alloc] initWithIncognito:isIncognito];

  self.viewController.defaultLeadingImage =
      GetOmniboxSuggestionIcon(DEFAULT_FAVICON);
  // TODO(crbug.com/1045047): Use HandlerForProtocol after commands protocol
  // clean up.
  self.viewController.dispatcher =
      static_cast<id<BrowserCommands, LoadQueryCommands, OmniboxCommands>>(
          self.browser->GetCommandDispatcher());
  self.viewController.delegate = self;
  self.mediator = [[OmniboxMediator alloc] init];
  self.mediator.templateURLService =
      ios::TemplateURLServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  self.mediator.faviconLoader =
      IOSChromeFaviconLoaderFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  self.mediator.consumer = self.viewController;

  DCHECK(self.editController);

  id<OmniboxCommands> focuser =
      static_cast<id<OmniboxCommands>>(self.browser->GetCommandDispatcher());
  _editView = std::make_unique<OmniboxViewIOS>(
      self.textField, self.editController, self.mediator,
      self.browser->GetBrowserState(), focuser);

  self.viewController.textChangeDelegate = _editView.get();

  // Configure the textfield.
  self.textField.suggestionCommandsEndpoint =
      static_cast<id<OmniboxSuggestionCommands>>(
          self.browser->GetCommandDispatcher());

  self.keyboardDelegate = [[OmniboxAssistiveKeyboardDelegateImpl alloc] init];
  // TODO(crbug.com/1045047): Use HandlerForProtocol after commands protocol
  // clean up.
  self.keyboardDelegate.dispatcher =
      static_cast<id<ApplicationCommands, BrowserCommands>>(
          self.browser->GetCommandDispatcher());
  self.keyboardDelegate.omniboxTextField = self.textField;
  ConfigureAssistiveKeyboardViews(self.textField, kDotComTLD,
                                  self.keyboardDelegate);
}

- (void)stop {
  _editView.reset();
  self.editController = nil;
  self.viewController = nil;
  self.mediator = nil;

  [NSNotificationCenter.defaultCenter removeObserver:self];
}

- (void)updateOmniboxState {
  _editView->UpdateAppearance();
}

- (BOOL)isOmniboxFirstResponder {
  return [self.textField isFirstResponder];
}

- (void)focusOmnibox {
  if (![self.textField isFirstResponder]) {
    base::RecordAction(base::UserMetricsAction("MobileOmniboxFocused"));
    [self.textField becomeFirstResponder];
  }
}

- (void)endEditing {
  [self.textField resignFirstResponder];
  _editView->EndEditing();
}

- (void)insertTextToOmnibox:(NSString*)text {
  [self.textField insertTextWhileEditing:text];
  // The call to |setText| shouldn't be needed, but without it the "Go" button
  // of the keyboard is disabled.
  [self.textField setText:text];
  // Notify the accessibility system to start reading the new contents of the
  // Omnibox.
  UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification,
                                  self.textField);
}

- (OmniboxPopupCoordinator*)createPopupCoordinator:
    (id<OmniboxPopupPresenterDelegate>)presenterDelegate {
  std::unique_ptr<OmniboxPopupViewIOS> popupView =
      std::make_unique<OmniboxPopupViewIOS>(_editView->model(),
                                            _editView.get());

  _editView->model()->set_popup_model(popupView->model());
  _editView->SetPopupProvider(popupView.get());

  OmniboxPopupCoordinator* coordinator = [[OmniboxPopupCoordinator alloc]
      initWithBaseViewController:nil
                         browser:self.browser
                       popupView:std::move(popupView)];
  coordinator.presenterDelegate = presenterDelegate;

  return coordinator;
}

- (UIViewController*)managedViewController {
  return self.viewController;
}

- (id<LocationBarOffsetProvider>)offsetProvider {
  return self.viewController;
}

- (id<EditViewAnimatee>)animatee {
  return self.viewController;
}

#pragma mark - OmniboxViewControllerDelegate

- (void)omniboxViewControllerTextInputModeDidChange:
    (OmniboxViewController*)omniboxViewController {
  _editView->UpdatePopupAppearance();
}

#pragma mark - private

// Convenience accessor.
- (OmniboxTextFieldIOS*)textField {
  return self.viewController.textField;
}

@end
