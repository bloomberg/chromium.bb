// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/app/remoting_view_controller.h"

#include <SystemConfiguration/SystemConfiguration.h>
#include <netinet/in.h>

#import "base/mac/bind_objc_block.h"
#import "ios/third_party/material_components_ios/src/components/AnimationTiming/src/MaterialAnimationTiming.h"
#import "ios/third_party/material_components_ios/src/components/AppBar/src/MaterialAppBar.h"
#import "ios/third_party/material_components_ios/src/components/Dialogs/src/MaterialDialogs.h"
#import "ios/third_party/material_components_ios/src/components/ShadowElevations/src/MaterialShadowElevations.h"
#import "ios/third_party/material_components_ios/src/components/ShadowLayer/src/MaterialShadowLayer.h"
#import "ios/third_party/material_components_ios/src/components/Snackbar/src/MaterialSnackbar.h"
#import "remoting/ios/app/app_delegate.h"
#import "remoting/ios/app/client_connection_view_controller.h"
#import "remoting/ios/app/host_collection_view_controller.h"
#import "remoting/ios/app/host_fetching_error_view_controller.h"
#import "remoting/ios/app/host_fetching_view_controller.h"
#import "remoting/ios/app/host_setup_view_controller.h"
#import "remoting/ios/app/host_view_controller.h"
#import "remoting/ios/app/refresh_control_provider.h"
#import "remoting/ios/app/remoting_menu_view_controller.h"
#import "remoting/ios/app/remoting_theme.h"
#import "remoting/ios/app/view_utils.h"
#import "remoting/ios/domain/client_session_details.h"
#import "remoting/ios/facade/remoting_service.h"

#include "base/strings/sys_string_conversions.h"
#include "remoting/base/oauth_token_getter.h"
#include "remoting/base/string_resources.h"
#include "remoting/client/connect_to_host_info.h"
#include "ui/base/l10n/l10n_util.h"

static CGFloat kHostInset = 5.f;

namespace {

#pragma mark - Network Reachability

enum class ConnectionType {
  UNKNOWN,
  NONE,
  WWAN,
  WIFI,
};

ConnectionType GetConnectionType() {
  // 0.0.0.0 is a special token that causes reachability to monitor the general
  // routing status of the device, both IPv4 and IPv6.
  struct sockaddr_in addr = {0};
  addr.sin_len = sizeof(addr);
  addr.sin_family = AF_INET;
  SCNetworkReachabilityRef reachability =
      SCNetworkReachabilityCreateWithAddress(
          kCFAllocatorDefault, reinterpret_cast<struct sockaddr*>(&addr));
  SCNetworkReachabilityFlags flags;
  BOOL success = SCNetworkReachabilityGetFlags(reachability, &flags);
  CFRelease(reachability);
  if (!success) {
    return ConnectionType::UNKNOWN;
  }
  BOOL isReachable = flags & kSCNetworkReachabilityFlagsReachable;
  BOOL needsConnection = flags & kSCNetworkReachabilityFlagsConnectionRequired;
  BOOL isNetworkReachable = isReachable && !needsConnection;

  if (!isNetworkReachable) {
    return ConnectionType::NONE;
  } else if (flags & kSCNetworkReachabilityFlagsIsWWAN) {
    return ConnectionType::WWAN;
  }
  return ConnectionType::WIFI;
}

}  // namespace

#pragma mark - RemotingViewController

@interface RemotingViewController ()<HostCollectionViewControllerDelegate,
                                     UIViewControllerAnimatedTransitioning,
                                     UIViewControllerTransitioningDelegate> {
  MDCDialogTransitionController* _dialogTransitionController;
  MDCAppBar* _appBar;
  HostCollectionViewController* _collectionViewController;
  HostFetchingViewController* _fetchingViewController;
  HostFetchingErrorViewController* _fetchingErrorViewController;
  HostSetupViewController* _setupViewController;
  RemotingService* _remotingService;

  NSArray<id<RemotingRefreshControl>>* _refreshControls;
}
@end

@implementation RemotingViewController

- (instancetype)init {
  UICollectionViewFlowLayout* layout =
      [[MDCCollectionViewFlowLayout alloc] init];
  layout.minimumInteritemSpacing = 0;
  CGFloat sectionInset = kHostInset * 2.f;
  [layout setSectionInset:UIEdgeInsetsMake(sectionInset, sectionInset,
                                           sectionInset, sectionInset)];
  self = [super init];
  if (self) {
    _remotingService = RemotingService.instance;

    __weak RemotingViewController* weakSelf = self;
    RemotingRefreshAction refreshAction = ^{
      [weakSelf didSelectRefresh];
    };

    _collectionViewController = [[HostCollectionViewController alloc]
        initWithCollectionViewLayout:layout];
    _collectionViewController.delegate = self;
    _collectionViewController.scrollViewDelegate = self.headerViewController;

    _fetchingViewController = [[HostFetchingViewController alloc] init];

    _fetchingErrorViewController =
        [[HostFetchingErrorViewController alloc] init];
    _fetchingErrorViewController.onRetryCallback = refreshAction;

    _setupViewController = [[HostSetupViewController alloc] init];
    _setupViewController.scrollViewDelegate = self.headerViewController;

    _appBar = [[MDCAppBar alloc] init];
    [self addChildViewController:_appBar.headerViewController];

    self.navigationItem.title =
        l10n_util::GetNSString(IDS_PRODUCT_NAME).lowercaseString;

    UIBarButtonItem* menuButton =
        [[UIBarButtonItem alloc] initWithImage:RemotingTheme.menuIcon
                                         style:UIBarButtonItemStyleDone
                                        target:self
                                        action:@selector(didSelectMenu)];
    remoting::SetAccessibilityInfoFromImage(menuButton);
    self.navigationItem.leftBarButtonItem = menuButton;

    _appBar.headerViewController.headerView.backgroundColor =
        RemotingTheme.hostListBackgroundColor;
    _appBar.navigationBar.backgroundColor =
        RemotingTheme.hostListBackgroundColor;
    MDCNavigationBarTextColorAccessibilityMutator* mutator =
        [[MDCNavigationBarTextColorAccessibilityMutator alloc] init];
    [mutator mutate:_appBar.navigationBar];

    MDCFlexibleHeaderView* headerView = self.headerViewController.headerView;
    headerView.backgroundColor = [UIColor clearColor];

    // Use a custom shadow under the flexible header.
    MDCShadowLayer* shadowLayer = [MDCShadowLayer layer];
    [headerView setShadowLayer:shadowLayer
        intensityDidChangeBlock:^(CALayer* layer, CGFloat intensity) {
          CGFloat elevation = MDCShadowElevationAppBar * intensity;
          [(MDCShadowLayer*)layer setElevation:elevation];
        }];

    _refreshControls = @[
      [[RefreshControlProvider instance]
          createForScrollView:_collectionViewController.collectionView
                  actionBlock:refreshAction],
      [[RefreshControlProvider instance]
          createForScrollView:_setupViewController.collectionView
                  actionBlock:refreshAction],
    ];
  }
  return self;
}

- (void)dealloc {
  [NSNotificationCenter.defaultCenter removeObserver:self];
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
         selector:@selector(hostListStateDidChangeNotification:)
             name:kHostListStateDidChange
           object:nil];

  [NSNotificationCenter.defaultCenter
      addObserver:self
         selector:@selector(hostListFetchDidFailNotification:)
             name:kHostListFetchDidFail
           object:nil];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];

  // Just in case the view controller misses the host list state event before
  // gthe listener is registered.
  [self refreshContent];
}

- (UIStatusBarStyle)preferredStatusBarStyle {
  return UIStatusBarStyleLightContent;
}

#pragma mark - Remoting Service Notifications

- (void)hostListStateDidChangeNotification:(NSNotification*)notification {
  [self refreshContent];
}

- (void)hostListFetchDidFailNotification:(NSNotification*)notification {
  HostListFetchFailureReason reason = (HostListFetchFailureReason)
      [notification.userInfo[kHostListFetchFailureReasonKey] integerValue];
  [self handleHostListFetchFailure:reason];
}

#pragma mark - HostCollectionViewControllerDelegate

- (void)didSelectCell:(HostCollectionViewCell*)cell
           completion:(void (^)())completionBlock {
  if (![cell.hostInfo isOnline]) {
    MDCSnackbarMessage* message = [[MDCSnackbarMessage alloc] init];
    message.text = l10n_util::GetNSString(IDS_HOST_OFFLINE_TOOLTIP);
    [MDCSnackbarManager showMessage:message];
    return;
  }

  void (^connectToHost)() = ^{
    [MDCSnackbarManager dismissAndCallCompletionBlocksWithCategory:nil];
    ClientConnectionViewController* clientConnectionViewController =
        [[ClientConnectionViewController alloc] initWithHostInfo:cell.hostInfo];
    [self.navigationController pushViewController:clientConnectionViewController
                                         animated:YES];
  };

  switch (GetConnectionType()) {
    case ConnectionType::WIFI:
      connectToHost();
      break;
    case ConnectionType::WWAN: {
      MDCAlertController* alert = [MDCAlertController
          alertControllerWithTitle:nil
                           message:l10n_util::GetNSString(
                                       IDS_MOBILE_NETWORK_WARNING)];

      MDCAlertAction* continueAction = [MDCAlertAction
          actionWithTitle:l10n_util::GetNSString(IDS_IDLE_CONTINUE)
                  handler:^(MDCAlertAction*) {
                    connectToHost();
                  }];
      [alert addAction:continueAction];

      MDCAlertAction* cancelAction =
          [MDCAlertAction actionWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                                  handler:nil];
      [alert addAction:cancelAction];

      [self presentViewController:alert animated:YES completion:nil];
      break;
    }
    default:
      [MDCSnackbarManager
          showMessage:[MDCSnackbarMessage
                          messageWithText:l10n_util::GetNSString(
                                              IDS_ERROR_NETWORK_ERROR)]];
  }
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

- (void)didSelectRefresh {
  // TODO(nicholss): Might want to rate limit this. Maybe remoting service
  // controls that.
  [_remotingService requestHostListFetch];
}

- (void)didSelectMenu {
  [AppDelegate.instance showMenuAnimated:YES];
}

- (void)refreshContent {
  if (_remotingService.hostListState == HostListStateFetching) {
    if (![self isAnyRefreshControlRefreshing]) {
      self.contentViewController = _fetchingViewController;
    }
    return;
  }

  if (_remotingService.hostListState == HostListStateNotFetched) {
    if (_remotingService.lastFetchFailureReason ==
        HostListFetchFailureReasonNoFailure) {
      self.contentViewController = nil;
    } else {
      // hostListFetchDidFailNotification might miss the first failure happened
      // before the notification is registered. This logic covers that.
      [self handleHostListFetchFailure:_remotingService.lastFetchFailureReason];
    }
    return;
  }

  DCHECK(_remotingService.hostListState == HostListStateFetched);

  [self stopAllRefreshControls];

  UICollectionViewController* contentViewController;
  if (_remotingService.hosts.count > 0) {
    [_collectionViewController.collectionView reloadData];
    contentViewController = _collectionViewController;
  } else {
    contentViewController = _setupViewController;
  }
  self.headerViewController.headerView.trackingScrollView =
      contentViewController.collectionView;
  self.contentViewController = contentViewController;
  self.contentViewController.view.frame = self.view.bounds;

  if (@available(iOS 11.0, *)) {
    // TODO(crbug.com/766892): This is a workaround for material component bug
    // https://github.com/material-components/material-components-ios/issues/1962
    // Remove once it is resolved.
    self.headerViewController.headerView.trackingScrollView
        .contentInsetAdjustmentBehavior =
        UIScrollViewContentInsetAdjustmentNever;
  }
}

- (void)handleHostListFetchFailure:(HostListFetchFailureReason)reason {
  int messageId;
  switch (reason) {
    case HostListFetchFailureReasonNetworkError:
      messageId = IDS_ERROR_NETWORK_ERROR;
      break;
    case HostListFetchFailureReasonAuthError:
      messageId = IDS_ERROR_OAUTH_TOKEN_INVALID;
      break;
    default:
      NOTREACHED();
      return;
  }
  NSString* errorText = l10n_util::GetNSString(messageId);
  if ([self isAnyRefreshControlRefreshing]) {
    // User could just try pull-to-refresh again to refresh. We just need to
    // show the error as a toast.
    [MDCSnackbarManager
        showMessage:[MDCSnackbarMessage messageWithText:errorText]];
    [self stopAllRefreshControls];
    return;
  }

  // Pull-to-refresh is not available. We need to show a dedicated view to allow
  // user to retry.

  // Dismiss snackbars and hide the SSO menu so that the accessibility focus
  // can shift into the label.
  [MDCSnackbarManager dismissAndCallCompletionBlocksWithCategory:nil];
  [AppDelegate.instance hideMenuAnimated:YES];

  _fetchingErrorViewController.label.text = errorText;
  remoting::SetAccessibilityFocusElement(_fetchingErrorViewController.label);
  self.contentViewController = _fetchingErrorViewController;
}

- (BOOL)isAnyRefreshControlRefreshing {
  for (id<RemotingRefreshControl> control in _refreshControls) {
    if (control.isRefreshing) {
      return YES;
    }
  }
  return NO;
}

- (void)stopAllRefreshControls {
  for (id<RemotingRefreshControl> control in _refreshControls) {
    [control endRefreshing];
  }
}

@end
