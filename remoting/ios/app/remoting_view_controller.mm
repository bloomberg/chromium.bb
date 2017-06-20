// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/app/remoting_view_controller.h"

#import "base/mac/bind_objc_block.h"
#import "ios/third_party/material_components_ios/src/components/AnimationTiming/src/MaterialAnimationTiming.h"
#import "ios/third_party/material_components_ios/src/components/AppBar/src/MaterialAppBar.h"
#import "ios/third_party/material_components_ios/src/components/Dialogs/src/MaterialDialogs.h"
#import "ios/third_party/material_components_ios/src/components/Snackbar/src/MaterialSnackbar.h"
#import "remoting/ios/app/app_delegate.h"
#import "remoting/ios/app/client_connection_view_controller.h"
#import "remoting/ios/app/host_collection_view_controller.h"
#import "remoting/ios/app/host_view_controller.h"
#import "remoting/ios/app/remoting_menu_view_controller.h"
#import "remoting/ios/app/remoting_theme.h"
#import "remoting/ios/domain/client_session_details.h"
#import "remoting/ios/facade/remoting_authentication.h"
#import "remoting/ios/facade/remoting_service.h"

#include "base/strings/sys_string_conversions.h"
#include "remoting/base/oauth_token_getter.h"
#include "remoting/client/connect_to_host_info.h"

static CGFloat kHostInset = 5.f;

@interface RemotingViewController ()<HostCollectionViewControllerDelegate,
                                     UIViewControllerAnimatedTransitioning,
                                     UIViewControllerTransitioningDelegate> {
  bool _isAuthenticated;
  MDCDialogTransitionController* _dialogTransitionController;
  MDCAppBar* _appBar;
  HostCollectionViewController* _collectionViewController;
  RemotingService* _remotingService;
}
@end

// TODO(nicholss): Localize this file.
// TODO(nicholss): This file is not finished with integration, the app flow is
// still pending development.

@implementation RemotingViewController

- (instancetype)init {
  _isAuthenticated = NO;
  UICollectionViewFlowLayout* layout =
      [[MDCCollectionViewFlowLayout alloc] init];
  layout.minimumInteritemSpacing = 0;
  CGFloat sectionInset = kHostInset * 2.f;
  [layout setSectionInset:UIEdgeInsetsMake(sectionInset, sectionInset,
                                           sectionInset, sectionInset)];
  HostCollectionViewController* collectionVC = [
      [HostCollectionViewController alloc] initWithCollectionViewLayout:layout];
  self = [super initWithContentViewController:collectionVC];
  if (self) {
    _remotingService = [RemotingService SharedInstance];

    _collectionViewController = collectionVC;
    _collectionViewController.flexHeaderContainerViewController = self;
    _collectionViewController.delegate = self;

    _appBar = [[MDCAppBar alloc] init];
    [self addChildViewController:_appBar.headerViewController];

    self.navigationItem.title = @"Chrome Remote Desktop";

    UIBarButtonItem* menuButton =
        [[UIBarButtonItem alloc] initWithImage:RemotingTheme.menuIcon
                                         style:UIBarButtonItemStyleDone
                                        target:self
                                        action:@selector(didSelectMenu)];
    self.navigationItem.leftBarButtonItem = menuButton;

    UIBarButtonItem* refreshButton =
        [[UIBarButtonItem alloc] initWithImage:RemotingTheme.refreshIcon
                                         style:UIBarButtonItemStyleDone
                                        target:self
                                        action:@selector(didSelectRefresh)];
    self.navigationItem.rightBarButtonItem = refreshButton;

    _appBar.headerViewController.headerView.backgroundColor =
        RemotingTheme.hostListBackgroundColor;
    _appBar.navigationBar.backgroundColor =
        RemotingTheme.hostListBackgroundColor;
    MDCNavigationBarTextColorAccessibilityMutator* mutator =
        [[MDCNavigationBarTextColorAccessibilityMutator alloc] init];
    [mutator mutate:_appBar.navigationBar];
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  UIImage* image = [UIImage imageNamed:@"Background"];
  UIImageView* imageView = [[UIImageView alloc] initWithImage:image];
  [self.view addSubview:imageView];
  [self.view sendSubviewToBack:imageView];

  imageView.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [[imageView widthAnchor]
        constraintGreaterThanOrEqualToAnchor:[self.view widthAnchor]],
    [[imageView heightAnchor]
        constraintGreaterThanOrEqualToAnchor:[self.view heightAnchor]],
  ]];

  [_appBar addSubviewsToParent];

  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(hostsDidUpdateNotification:)
             name:kHostsDidUpdate
           object:nil];
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(userDidUpdateNotification:)
             name:kUserDidUpdate
           object:nil];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];

  [self nowAuthenticated:_remotingService.authentication.user.isAuthenticated];
  [self presentStatus];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  if (!_isAuthenticated) {
    [AppDelegate.instance presentSignInFlow];
    MDCSnackbarMessage* message = [[MDCSnackbarMessage alloc] init];
    message.text = @"Please login.";
    [MDCSnackbarManager showMessage:message];
  } else {
    [_remotingService requestHostListFetch];
  }
}

- (UIStatusBarStyle)preferredStatusBarStyle {
  return UIStatusBarStyleLightContent;
}

#pragma mark - Remoting Service Notifications

- (void)hostsDidUpdateNotification:(NSNotification*)notification {
  [_collectionViewController.collectionView reloadData];
}

- (void)userDidUpdateNotification:(NSNotification*)notification {
  [self nowAuthenticated:_remotingService.authentication.user.isAuthenticated];
}

#pragma mark - RemotingAuthenticationDelegate

- (void)nowAuthenticated:(BOOL)authenticated {
  if (authenticated) {
    MDCSnackbarMessage* message = [[MDCSnackbarMessage alloc] init];
    message.text = @"Logged In!";
    [MDCSnackbarManager showMessage:message];
  } else {
    MDCSnackbarMessage* message = [[MDCSnackbarMessage alloc] init];
    message.text = @"Not logged in.";
    [MDCSnackbarManager showMessage:message];
  }
  _isAuthenticated = authenticated;
  [_collectionViewController.collectionView reloadData];
}

#pragma mark - RemotingHostListDelegate

// TODO(nicholss): these need to be a stats change like "none, loading,
// updated"...
- (void)hostListUpdated {
  [_collectionViewController.collectionView reloadData];
}

#pragma mark - HostCollectionViewControllerDelegate

- (void)didSelectCell:(HostCollectionViewCell*)cell
           completion:(void (^)())completionBlock {
  if (![cell.hostInfo isOnline]) {
    MDCSnackbarMessage* message = [[MDCSnackbarMessage alloc] init];
    message.text = @"Host is offline.";
    [MDCSnackbarManager showMessage:message];
    return;
  }

  [MDCSnackbarManager dismissAndCallCompletionBlocksWithCategory:nil];
  ClientConnectionViewController* clientConnectionViewController =
      [[ClientConnectionViewController alloc] initWithHostInfo:cell.hostInfo];
  [self.navigationController pushViewController:clientConnectionViewController
                                       animated:YES];
  completionBlock();
}

- (NSInteger)getHostCount {
  return _remotingService.hosts.count;
}

- (HostInfo*)getHostAtIndexPath:(NSIndexPath*)path {
  return _remotingService.hosts[path.row];
}

#pragma mark - UIViewControllerTransitioningDelegate

- (nullable id<UIViewControllerAnimatedTransitioning>)
animationControllerForPresentedController:(UIViewController*)presented
                     presentingController:(UIViewController*)presenting
                         sourceController:(UIViewController*)source {
  // TODO(nicholss): Not implemented yet.
  return nil;
}

- (nullable id<UIViewControllerAnimatedTransitioning>)
animationControllerForDismissedController:(UIViewController*)dismissed {
  return self;
}

#pragma mark - UIViewControllerAnimatedTransitioning

- (void)animateTransition:
    (id<UIViewControllerContextTransitioning>)transitionContext {
}

- (NSTimeInterval)transitionDuration:
    (id<UIViewControllerContextTransitioning>)transitionContext {
  return 0.2;
}

#pragma mark - Private

- (void)closeViewController {
  [self dismissViewControllerAnimated:true completion:nil];
}

- (void)didSelectRefresh {
  // TODO(nicholss): Might want to rate limit this. Maybe remoting service
  // controls that.
  [_remotingService requestHostListFetch];
}

- (void)didSelectMenu {
  [AppDelegate.instance showMenuAnimated:YES];
}

- (void)presentStatus {
  MDCSnackbarMessage* message = [[MDCSnackbarMessage alloc] init];
  if (_isAuthenticated) {
    message.text = [NSString
        stringWithFormat:@"Currently signed in as %@.",
                         _remotingService.authentication.user.userEmail];
    [MDCSnackbarManager showMessage:message];
  }
}

@end
