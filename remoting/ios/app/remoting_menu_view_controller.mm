// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/app/remoting_menu_view_controller.h"

#import "ios/third_party/material_components_ios/src/components/AppBar/src/MaterialAppBar.h"
#import "ios/third_party/material_components_ios/src/components/Buttons/src/MaterialButtons.h"
#import "remoting/ios/app/remoting_theme.h"
#import "remoting/ios/facade/remoting_authentication.h"
#import "remoting/ios/facade/remoting_service.h"

#include "base/strings/stringprintf.h"
#include "google_apis/google_api_keys.h"
#include "net/base/escape.h"
#include "remoting/base/string_resources.h"
#include "ui/base/l10n/l10n_util.h"

// TODO(nicholss): This should be generated from a remoting/base class:

static NSString* const kReusableIdentifierItem =
    @"remotingMenuViewControllerItem";
static UIColor* kBackgroundColor =
    [UIColor colorWithRed:0.f green:0.67f blue:0.55f alpha:1.f];

namespace {
const char kChromotingAuthScopeValues[] =
    "https://www.googleapis.com/auth/chromoting "
    "https://www.googleapis.com/auth/googletalk "
    "https://www.googleapis.com/auth/userinfo.email";

std::string GetAuthorizationCodeUri() {
  // Replace space characters with a '+' sign when formatting.
  bool use_plus = true;
  return base::StringPrintf(
      "https://accounts.google.com/o/oauth2/auth"
      "?scope=%s"
      "&redirect_uri=https://chromoting-oauth.talkgadget.google.com/"
      "talkgadget/oauth/chrome-remote-desktop/dev"
      "&response_type=code"
      "&client_id=%s"
      "&access_type=offline"
      "&approval_prompt=force",
      net::EscapeUrlEncodedData(kChromotingAuthScopeValues, use_plus).c_str(),
      net::EscapeUrlEncodedData(
          google_apis::GetOAuth2ClientID(google_apis::CLIENT_REMOTING),
          use_plus)
          .c_str());
}

}  // namespace

@interface RemotingMenuViewController () {
  MDCAppBar* _appBar;
  NSMutableArray* _content;
}
@end

// This is the chromium version of the menu view controller. This will
// launch a web view to login and collect an oauth token to be able to login to
// the app without the standard google login flow.
//
// This class is majority boiler plate code to get a collection view.
// It will be replaced with a sidebar-like view in the future, but in
// chromium this is how we get an oauth token to login to the app.
//
// Note: this class is not localized, it will not be shipped to production.
//
// TODO(nicholss): This class needs to be split into a shareable view
// for chromium and prod to share.
//
@implementation RemotingMenuViewController

- (id)init {
  self = [super init];
  if (self) {
    self.title = l10n_util::GetNSString(IDS_SETTINGS_BUTTON);

    _appBar = [[MDCAppBar alloc] init];
    [self addChildViewController:_appBar.headerViewController];

    _appBar.headerViewController.headerView.backgroundColor = kBackgroundColor;
    _appBar.navigationBar.tintColor = [UIColor whiteColor];
    _appBar.navigationBar.titleTextAttributes =
        @{NSForegroundColorAttributeName : [UIColor whiteColor]};
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  _appBar.headerViewController.headerView.trackingScrollView =
      self.collectionView;
  [_appBar addSubviewsToParent];

  UIBarButtonItem* backButton =
      [[UIBarButtonItem alloc] initWithImage:RemotingTheme.backIcon
                                       style:UIBarButtonItemStyleDone
                                      target:self
                                      action:@selector(didTapBack:)];
  self.navigationItem.leftBarButtonItem = backButton;
  self.navigationItem.rightBarButtonItem = nil;

  [self.collectionView registerClass:[MDCCollectionViewTextCell class]
          forCellWithReuseIdentifier:kReusableIdentifierItem];

  [self.collectionView registerClass:[MDCCollectionViewTextCell class]
          forSupplementaryViewOfKind:UICollectionElementKindSectionHeader
                 withReuseIdentifier:UICollectionElementKindSectionHeader];

  self.styler.cellStyle = MDCCollectionViewCellStyleCard;

  _content = [NSMutableArray array];
  [_content addObject:@[
    l10n_util::GetNSString(IDS_SIGN_IN_BUTTON),
    l10n_util::GetNSString(IDS_SIGN_OUT_BUTTON)
  ]];
}

#pragma mark - UICollectionViewDataSource

- (NSInteger)numberOfSectionsInCollectionView:
    (UICollectionView*)collectionView {
  return (NSInteger)[_content count];
}

- (NSInteger)collectionView:(UICollectionView*)collectionView
     numberOfItemsInSection:(NSInteger)section {
  return (NSInteger)[_content[(NSUInteger)section] count];
}

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  MDCCollectionViewTextCell* cell = [collectionView
      dequeueReusableCellWithReuseIdentifier:kReusableIdentifierItem
                                forIndexPath:indexPath];
  cell.textLabel.text =
      _content[(NSUInteger)indexPath.section][(NSUInteger)indexPath.item];

  if (indexPath.section == 0 && indexPath.item == 0) {
    MDCRaisedButton* accessCodeButton = [[MDCRaisedButton alloc] init];
    [accessCodeButton setTitle:@"Get Access Code"
                      forState:UIControlStateNormal];
    [accessCodeButton sizeToFit];
    [accessCodeButton addTarget:self
                         action:@selector(didTapGetAccessCode:)
               forControlEvents:UIControlEventTouchUpInside];
    accessCodeButton.translatesAutoresizingMaskIntoConstraints = NO;
    cell.accessoryView = accessCodeButton;
  } else if (indexPath.section == 0 && indexPath.item == 1) {
    MDCRaisedButton* logoutButton = [[MDCRaisedButton alloc] init];
    [logoutButton setTitle:l10n_util::GetNSString(IDS_SIGN_OUT_BUTTON)
                  forState:UIControlStateNormal];
    [logoutButton sizeToFit];
    [logoutButton addTarget:self
                     action:@selector(didTapLogout:)
           forControlEvents:UIControlEventTouchUpInside];
    logoutButton.translatesAutoresizingMaskIntoConstraints = NO;
    cell.accessoryView = logoutButton;
  }

  return cell;
}

- (UICollectionReusableView*)collectionView:(UICollectionView*)collectionView
          viewForSupplementaryElementOfKind:(NSString*)kind
                                atIndexPath:(NSIndexPath*)indexPath {
  MDCCollectionViewTextCell* supplementaryView =
      [collectionView dequeueReusableSupplementaryViewOfKind:kind
                                         withReuseIdentifier:kind
                                                forIndexPath:indexPath];
  if ([kind isEqualToString:UICollectionElementKindSectionHeader]) {
    if (indexPath.section == 0) {
      supplementaryView.textLabel.text = @"Account";
    }
    supplementaryView.textLabel.textColor = kBackgroundColor;
  }
  return supplementaryView;
}

#pragma mark - <UICollectionViewDelegateFlowLayout>

- (CGSize)collectionView:(UICollectionView*)collectionView
                             layout:
                                 (UICollectionViewLayout*)collectionViewLayout
    referenceSizeForHeaderInSection:(NSInteger)section {
  return CGSizeMake(collectionView.bounds.size.width,
                    MDCCellDefaultOneLineHeight);
}

#pragma mark - Private

- (void)didTapBack:(id)button {
  [self dismissViewControllerAnimated:YES completion:nil];
}

- (void)didTapGetAccessCode:(id)sender {
  NSString* authUri =
      [NSString stringWithCString:GetAuthorizationCodeUri().c_str()
                         encoding:[NSString defaultCStringEncoding]];
  if (@available(iOS 10, *)) {
    [[UIApplication sharedApplication] openURL:[NSURL URLWithString:authUri]
                                       options:@{}
                             completionHandler:nil];
  }
#if !defined(__IPHONE_10_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_10_0
  else {
    [[UIApplication sharedApplication] openURL:[NSURL URLWithString:authUri]];
  }
#endif
}

- (void)didTapLogout:(id)sender {
  [RemotingService.instance.authentication logout];
}

@end
