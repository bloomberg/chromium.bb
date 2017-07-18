// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/app/app_delegate.h"

#include "base/logging.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#import "remoting/ios/app/app_view_controller.h"
#import "remoting/ios/app/first_launch_view_presenter.h"
#import "remoting/ios/app/help_and_feedback.h"
#import "remoting/ios/app/help_view_controller.h"
#import "remoting/ios/app/remoting_view_controller.h"
#import "remoting/ios/app/user_status_presenter.h"
#import "remoting/ios/app/web_view_controller.h"
#import "remoting/ios/facade/remoting_oauth_authentication.h"

@interface AppDelegate ()<FirstLaunchViewControllerDelegate> {
  AppViewController* _appViewController;
  FirstLaunchViewPresenter* _firstLaunchViewPresenter;
}
@end

// TODO(nicholss): There is no FAQ page at the moment.
static NSString* const kFAQsUrl =
    @"https://support.google.com/chrome/answer/1649523?co=GENIE.Platform%3DiOS";

@implementation AppDelegate

@synthesize window = _window;

- (BOOL)application:(UIApplication*)application
    willFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
  self.window =
      [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
  self.window.backgroundColor = [UIColor whiteColor];
  return YES;
}

- (BOOL)application:(UIApplication*)application
    didFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
  [self launchRootViewController];
  return YES;
}

#ifndef NDEBUG
- (BOOL)application:(UIApplication*)application handleOpenURL:(NSURL*)url {
  DCHECK([RemotingService.instance.authentication
      isKindOfClass:[RemotingOAuthAuthentication class]]);

  NSMutableDictionary* components = [[NSMutableDictionary alloc] init];
  NSArray* urlComponents = [[url query] componentsSeparatedByString:@"&"];

  for (NSString* componentPair in urlComponents) {
    NSArray* pair = [componentPair componentsSeparatedByString:@"="];
    NSString* key = [[pair firstObject] stringByRemovingPercentEncoding];
    NSString* value = [[pair lastObject] stringByRemovingPercentEncoding];
    [components setObject:value forKey:key];
  }
  NSString* authorizationCode = [components objectForKey:@"code"];

  [(RemotingOAuthAuthentication*)RemotingService.instance.authentication
      authenticateWithAuthorizationCode:authorizationCode];

  [self launchRootViewController];
  return YES;
}
#endif  // ifndef NDEBUG

#pragma mark - Public
- (void)showMenuAnimated:(BOOL)animated {
  DCHECK(_appViewController != nil);
  [_appViewController showMenuAnimated:animated];
}

- (void)hideMenuAnimated:(BOOL)animated {
  DCHECK(_appViewController != nil);
  [_appViewController hideMenuAnimated:animated];
}

#pragma mark - Properties

+ (AppDelegate*)instance {
  id<UIApplicationDelegate> delegate = UIApplication.sharedApplication.delegate;
  DCHECK([delegate isKindOfClass:AppDelegate.class]);
  return (AppDelegate*)delegate;
}

#pragma mark - Private

- (void)launchRootViewController {
  RemotingViewController* vc = [[RemotingViewController alloc] init];
  UINavigationController* navController =
      [[UINavigationController alloc] initWithRootViewController:vc];
  navController.navigationBarHidden = true;
  _appViewController =
      [[AppViewController alloc] initWithMainViewController:navController];
  _firstLaunchViewPresenter =
      [[FirstLaunchViewPresenter alloc] initWithNavController:navController
                                       viewControllerDelegate:self];
  if (![RemotingService.instance.authentication.user isAuthenticated]) {
    [_firstLaunchViewPresenter presentView];
  }
  self.window.rootViewController = _appViewController;
  [self.window makeKeyAndVisible];
  [UserStatusPresenter.instance start];
}

#pragma mark - AppDelegate

- (void)navigateToFAQs:(UINavigationController*)navigationController {
  WebViewController* viewController =
      [[WebViewController alloc] initWithUrl:kFAQsUrl title:@"FAQs"];
  [navigationController pushViewController:viewController animated:YES];
}

- (void)navigateToHelpCenter:(UINavigationController*)navigationController {
  [navigationController pushViewController:[[HelpViewController alloc] init]
                                  animated:YES];
}

- (void)presentHelpCenter {
  UINavigationController* navController = [[UINavigationController alloc]
      initWithRootViewController:[[HelpViewController alloc] init]];
  [AppDelegate.topPresentingVC presentViewController:navController
                                            animated:YES
                                          completion:nil];
}

- (void)presentFeedbackFlowWithContext:(NSString*)context {
  [HelpAndFeedback.instance presentFeedbackFlowWithContext:context];
}

- (void)emailSetupInstructions {
  NSLog(@"TODO: emailSetupInstructions");
}

#pragma mark - FirstLaunchViewPresenterDelegate

- (void)presentSignInFlow {
  DCHECK(_appViewController);
  [_appViewController presentSignInFlow];
}

#pragma mark - Private

+ (UIViewController*)topPresentingVC {
  UIViewController* topController =
      UIApplication.sharedApplication.keyWindow.rootViewController;

  while (topController.presentedViewController) {
    topController = topController.presentedViewController;
  }

  return topController;
}

@end
