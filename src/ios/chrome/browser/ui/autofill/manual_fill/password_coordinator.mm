// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/password_coordinator.h"

#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/password_store.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_injection_handler.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/password_list_delegate.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/password_mediator.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/password_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ManualFillPasswordCoordinator ()<PasswordListDelegate>

// Fetches and filters the passwords for the view controller.
@property(nonatomic, strong) ManualFillPasswordMediator* passwordMediator;

// The view controller presented above the keyboard where the user can select
// one of their passwords.
@property(nonatomic, strong) PasswordViewController* passwordViewController;

// The view controller modally presented where the user can select one of their
// passwords. Owned by the view controllers hierarchy.
@property(nonatomic, weak) PasswordViewController* allPasswordsViewController;

// The object in charge of interacting with the web view. Used to fill the data
// in the forms.
@property(nonatomic, strong)
    ManualFillInjectionHandler* manualFillInjectionHandler;

@end

@implementation ManualFillPasswordCoordinator

@synthesize allPasswordsViewController = _allPasswordsViewController;
@synthesize manualFillInjectionHandler = _manualFillInjectionHandler;
@synthesize passwordMediator = _passwordMediator;
@synthesize passwordViewController = _passwordViewController;

- (instancetype)
initWithBaseViewController:(UIViewController*)viewController
              browserState:(ios::ChromeBrowserState*)browserState
              webStateList:(WebStateList*)webStateList
          injectionHandler:(ManualFillInjectionHandler*)injectionHandler {
  self = [super initWithBaseViewController:viewController
                              browserState:browserState];
  if (self) {
    _passwordViewController =
        [[PasswordViewController alloc] initWithSearchController:nil];

    _manualFillInjectionHandler = injectionHandler;

    auto passwordStore = IOSChromePasswordStoreFactory::GetForBrowserState(
        browserState, ServiceAccessType::EXPLICIT_ACCESS);
    _passwordMediator =
        [[ManualFillPasswordMediator alloc] initWithWebStateList:webStateList
                                                   passwordStore:passwordStore];
    _passwordMediator.consumer = _passwordViewController;
    _passwordMediator.navigationDelegate = self;
    _passwordMediator.contentDelegate = _manualFillInjectionHandler;
  }
  return self;
}

- (void)stop {
  [self.passwordViewController.view removeFromSuperview];
  [self.allPasswordsViewController dismissViewControllerAnimated:YES
                                                      completion:nil];
}

- (UIViewController*)viewController {
  return self.passwordViewController;
}

#pragma mark - PasswordListDelegate

- (void)openAllPasswordsList {
  UISearchController* searchController =
      [[UISearchController alloc] initWithSearchResultsController:nil];
  searchController.searchResultsUpdater = self.passwordMediator;

  PasswordViewController* allPasswordsViewController = [
      [PasswordViewController alloc] initWithSearchController:searchController];
  self.passwordMediator.disableFilter = YES;
  self.passwordMediator.consumer = allPasswordsViewController;

  UINavigationController* navigationController = [[UINavigationController alloc]
      initWithRootViewController:allPasswordsViewController];
  if (@available(iOS 11, *)) {
    navigationController.navigationBar.prefersLargeTitles = YES;
  }
  self.allPasswordsViewController = allPasswordsViewController;
  [self.baseViewController presentViewController:navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)dismissPresentedViewController {
  [self.allPasswordsViewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
}

- (void)openPasswordSettings {
  [self.delegate openPasswordSettings];
}

@end
