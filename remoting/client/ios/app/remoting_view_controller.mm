// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/client/ios/app/remoting_view_controller.h"

#import "ios/third_party/material_components_ios/src/components/AnimationTiming/src/MaterialAnimationTiming.h"
#import "ios/third_party/material_components_ios/src/components/AppBar/src/MaterialAppBar.h"
#import "ios/third_party/material_components_ios/src/components/Dialogs/src/MaterialDialogs.h"
#import "ios/third_party/material_components_ios/src/components/Snackbar/src/MaterialSnackbar.h"
#import "remoting/client/ios/app/host_collection_view_controller.h"
#import "remoting/client/ios/app/host_view_controller.h"
#import "remoting/client/ios/app/pin_entry_view_controller.h"
#import "remoting/client/ios/app/remoting_settings_view_controller.h"
#import "remoting/client/ios/facade/remoting_service.h"

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
      [[UICollectionViewFlowLayout alloc] init];
  layout.minimumInteritemSpacing = 0;
  CGFloat sectionInset = kHostInset * 2.f;
  [layout setSectionInset:UIEdgeInsetsMake(sectionInset, sectionInset,
                                           sectionInset, sectionInset)];
  HostCollectionViewController* collectionVC = [
      [HostCollectionViewController alloc] initWithCollectionViewLayout:layout];
  self = [super initWithContentViewController:collectionVC];
  if (self) {
    _remotingService = [RemotingService SharedInstance];
    [_remotingService setAuthenticationDelegate:self];

    _collectionViewController = collectionVC;
    _collectionViewController.flexHeaderContainerViewController = self;
    _collectionViewController.delegate = self;

    _appBar = [[MDCAppBar alloc] init];
    [self addChildViewController:_appBar.headerViewController];

    _appBar.headerViewController.headerView.backgroundColor =
        [UIColor clearColor];
    _appBar.navigationBar.tintColor = [UIColor whiteColor];

    UIBarButtonItem* menuButton =
        [[UIBarButtonItem alloc] initWithImage:[UIImage imageNamed:@"Settings"]
                                         style:UIBarButtonItemStyleDone
                                        target:self
                                        action:@selector(didSelectSettings)];
    self.navigationItem.leftBarButtonItem = menuButton;

    UIBarButtonItem* refreshButton =
        [[UIBarButtonItem alloc] initWithTitle:@"Refresh"
                                         style:UIBarButtonItemStyleDone
                                        target:self
                                        action:@selector(didSelectRefresh)];
    self.navigationItem.rightBarButtonItem = refreshButton;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  [_appBar addSubviewsToParent];
  [self presentStatus];
}

- (void)viewDidAppear:(BOOL)animated {
  if (!_isAuthenticated) {
    // TODO(nicholss): This is used as a demo of the app functionality for the
    // moment but the real app will force the login flow if unauthenticated.
    //[self didSelectSettings];
    [self didSelectRefresh];
    MDCSnackbarMessage* message = [[MDCSnackbarMessage alloc] init];
    message.text = @"Please login.";
    [MDCSnackbarManager showMessage:message];
  }
}

#pragma mark - RemotingAuthenticationDelegate

- (void)nowAuthenticated:(BOOL)authenticated {
  if (authenticated) {
    MDCSnackbarMessage* message = [[MDCSnackbarMessage alloc] init];
    message.text = @"Logged In!";
    [MDCSnackbarManager showMessage:message];
    [_remotingService setHostListDelegate:self];
  } else {
    MDCSnackbarMessage* message = [[MDCSnackbarMessage alloc] init];
    message.text = @"Not logged in.";
    [MDCSnackbarManager showMessage:message];
    [_remotingService setHostListDelegate:nil];
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
  HostViewController* hostVC = [HostViewController new];
  [self presentViewController:hostVC animated:YES completion:nil];
}

- (NSInteger)getHostCount {
  NSArray<HostInfo*>* hosts = [_remotingService getHosts];
  return [hosts count];
}

- (HostInfo*)getHostAtIndexPath:(NSIndexPath*)path {
  NSArray<HostInfo*>* hosts = [_remotingService getHosts];
  return hosts[path.row];
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
  // TODO(nicholss) implement this.
  NSLog(@"Should refresh...");
  _dialogTransitionController = [[MDCDialogTransitionController alloc] init];
  PinEntryViewController* vc =
      [[PinEntryViewController alloc] initWithCallback:nil];
  vc.modalPresentationStyle = UIModalPresentationCustom;
  vc.transitioningDelegate = _dialogTransitionController;
  [self presentViewController:vc animated:YES completion:nil];
}

- (void)didSelectSettings {
  RemotingSettingsViewController* settingsVC =
      [RemotingSettingsViewController new];
  [self presentViewController:settingsVC animated:YES completion:nil];
}

- (void)presentStatus {
  MDCSnackbarMessage* message = [[MDCSnackbarMessage alloc] init];
  if (_isAuthenticated) {
    UserInfo* user = [_remotingService getUser];
    message.text = [NSString
        stringWithFormat:@"Currently signed in as %@.", [user userEmail]];
    [MDCSnackbarManager showMessage:message];
  }
}

@end
