// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/client/ios/app/remoting_view_controller.h"

#import "base/mac/bind_objc_block.h"
#import "ios/third_party/material_components_ios/src/components/AnimationTiming/src/MaterialAnimationTiming.h"
#import "ios/third_party/material_components_ios/src/components/AppBar/src/MaterialAppBar.h"
#import "ios/third_party/material_components_ios/src/components/Dialogs/src/MaterialDialogs.h"
#import "ios/third_party/material_components_ios/src/components/Snackbar/src/MaterialSnackbar.h"
#import "remoting/client/ios/app/host_collection_view_controller.h"
#import "remoting/client/ios/app/host_view_controller.h"
#import "remoting/client/ios/app/pin_entry_view_controller.h"
#import "remoting/client/ios/app/remoting_settings_view_controller.h"
#import "remoting/client/ios/facade/remoting_service.h"
#import "remoting/client/ios/session/remoting_client.h"

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
    [self didSelectSettings];
    // [self didSelectRefresh];
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
  RemotingClient* client = [[RemotingClient alloc] init];

  [_remotingService
      callbackWithAccessToken:base::BindBlockArc(^(
                                  remoting::OAuthTokenGetter::Status status,
                                  const std::string& user_email,
                                  const std::string& access_token) {
        // TODO(nicholss): Check status.
        HostInfo* hostInfo = cell.hostInfo;
        DCHECK(hostInfo);
        DCHECK(hostInfo.jabberId);
        DCHECK(hostInfo.hostId);
        DCHECK(hostInfo.publicKey);

        remoting::ConnectToHostInfo info;
        info.username = user_email;
        info.auth_token = access_token;
        info.host_jid = base::SysNSStringToUTF8(hostInfo.jabberId);
        info.host_id = base::SysNSStringToUTF8(hostInfo.hostId);
        info.host_pubkey = base::SysNSStringToUTF8(hostInfo.publicKey);
        // TODO(nicholss): If iOS supports pairing, pull the stored data and
        // insert it here.
        info.pairing_id = "";
        info.pairing_secret = "";

        // TODO(nicholss): I am not sure about the following fields yet.
        // info.capabilities =
        // info.flags =
        // info.host_version =
        // info.host_os =
        // info.host_os_version =
        [client connectToHost:info];
      })];

  HostViewController* hostViewController =
      [[HostViewController alloc] initWithClient:client];

  // TODO(nicholss): Add feedback on status of request.
  [self presentViewController:hostViewController animated:YES completion:nil];

  completionBlock();
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
