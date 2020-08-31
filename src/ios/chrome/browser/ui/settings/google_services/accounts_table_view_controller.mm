// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services/accounts_table_view_controller.h"

#import "base/mac/foundation_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/driver/sync_service.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#include "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_identity_service_observer_bridge.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/alert_coordinator/action_sheet_coordinator.h"
#import "ios/chrome/browser/ui/alert_coordinator/alert_coordinator.h"
#import "ios/chrome/browser/ui/authentication/cells/table_view_account_item.h"
#import "ios/chrome/browser/ui/authentication/resized_avatar_cache.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/commands/show_signin_command.h"
#import "ios/chrome/browser/ui/icons/chrome_icon.h"
#import "ios/chrome/browser/ui/settings/google_services/accounts_table_view_controller_constants.h"
#import "ios/chrome/browser/ui/settings/sync/utils/sync_util.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/ui/table_view/table_view_model.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/images/branded_image_provider.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity_browser_opener.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"
#import "net/base/mac/url_conversions.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using signin_metrics::AccessPoint;
using signin_metrics::PromoAction;

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierAccounts = kSectionIdentifierEnumZero,
  SectionIdentifierSync,
  SectionIdentifierSignOut,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeAccount = kItemTypeEnumZero,
  ItemTypeAddAccount,
  // Provides experimental and production sign out items. In the experimental
  // item, this type is used only for non-managed accounts.
  ItemTypeSignOut,
  // Experimental sign out item that clears Chrome data. Used for both managed
  // and non-managed accounts.
  ItemTypeSignOutAndClearData,
  ItemTypeHeader,
  // Detailed description of the actions taken by sign out e.g. turning off sync
  // and clearing Chrome data.
  ItemTypeSignOutManagedAccountFooter,
  // Detailed description of the actions taken by sign out, e.g. turning off
  // sync.
  ItemTypeSignOutNonManagedAccountFooter,
};

}  // namespace

@interface AccountsTableViewController () <
    ChromeIdentityServiceObserver,
    ChromeIdentityBrowserOpener,
    IdentityManagerObserverBridgeDelegate> {
  Browser* _browser;
  BOOL _closeSettingsOnAddAccount;
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserver;
  // Modal alert for sign out.
  AlertCoordinator* _alertCoordinator;
  // Whether an authentication operation is in progress (e.g switch accounts,
  // sign out).
  BOOL _authenticationOperationInProgress;
  // Whether the view controller is currently being dismissed and new dismiss
  // requests should be ignored.
  BOOL _isBeingDismissed;
  ios::DismissASMViewControllerBlock _dimissAccountDetailsViewControllerBlock;
  ResizedAvatarCache* _avatarCache;
  std::unique_ptr<ChromeIdentityServiceObserverBridge> _identityServiceObserver;

  // Enable lookup of item corresponding to a given identity GAIA ID string.
  NSDictionary<NSString*, TableViewItem*>* _identityMap;
}

// Stops observing browser state services. This is required during the shutdown
// phase to avoid observing services for a browser state that is being killed.
- (void)stopBrowserStateServiceObservers;

@end

@implementation AccountsTableViewController

@synthesize dispatcher = _dispatcher;

- (instancetype)initWithBrowser:(Browser*)browser
      closeSettingsOnAddAccount:(BOOL)closeSettingsOnAddAccount {
  DCHECK(browser);
  DCHECK(!browser->GetBrowserState()->IsOffTheRecord());
  UITableViewStyle style = base::FeatureList::IsEnabled(kSettingsRefresh)
                               ? UITableViewStylePlain
                               : UITableViewStyleGrouped;
  self = [super initWithStyle:style];
  if (self) {
    _browser = browser;
    _closeSettingsOnAddAccount = closeSettingsOnAddAccount;
    _identityManagerObserver =
        std::make_unique<signin::IdentityManagerObserverBridge>(
            IdentityManagerFactory::GetForBrowserState(
                _browser->GetBrowserState()),
            self);
    _avatarCache = [[ResizedAvatarCache alloc] init];
    _identityServiceObserver.reset(
        new ChromeIdentityServiceObserverBridge(self));
  }

  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.accessibilityIdentifier = kSettingsAccountsTableViewId;

  [self loadModel];
}

- (void)stopBrowserStateServiceObservers {
  _identityManagerObserver.reset();
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  base::RecordAction(base::UserMetricsAction("MobileAccountsSettingsClose"));
}

- (void)settingsWillBeDismissed {
  [_alertCoordinator stop];
  [self stopBrowserStateServiceObservers];
}

#pragma mark - SettingsRootTableViewController

- (void)reloadData {
  if (![self authService] -> IsAuthenticated()) {
    // This accounts table view will be popped or dismissed when the user
    // is signed out. Avoid reloading it in that case as that would lead to an
    // empty table view.
    return;
  }
  [super reloadData];
}

- (void)loadModel {
  // Update the title with the name with the currently signed-in account.
  ChromeIdentity* authenticatedIdentity =
      [self authService] -> GetAuthenticatedIdentity();
  NSString* title = nil;
  if (authenticatedIdentity) {
    title = [authenticatedIdentity userFullName];
    if (!title) {
      title = [authenticatedIdentity userEmail];
    }
  }
  self.title = title;

  [super loadModel];

  if (![self authService] -> IsAuthenticated())
    return;

  TableViewModel* model = self.tableViewModel;

  NSMutableDictionary<NSString*, TableViewItem*>* mutableIdentityMap =
      [[NSMutableDictionary alloc] init];

  // Account cells.
  [model addSectionWithIdentifier:SectionIdentifierAccounts];
  [model setHeader:[self header]
      forSectionWithIdentifier:SectionIdentifierAccounts];
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForBrowserState(_browser->GetBrowserState());

  NSString* authenticatedEmail = [authenticatedIdentity userEmail];
  for (const auto& account : identityManager->GetAccountsWithRefreshTokens()) {
    ChromeIdentity* identity = ios::GetChromeBrowserProvider()
                                   ->GetChromeIdentityService()
                                   ->GetIdentityWithGaiaID(account.gaia);
    // TODO(crbug.com/1081274): This re-ordering will be redundant once we
    // apply ordering changes to the account reconciler.
    TableViewItem* item = [self accountItem:identity];
    if ([identity.userEmail isEqual:authenticatedEmail]) {
      [model insertItem:item
          inSectionWithIdentifier:SectionIdentifierAccounts
                          atIndex:0];
    } else {
      [model addItem:item toSectionWithIdentifier:SectionIdentifierAccounts];
    }

    [mutableIdentityMap setObject:item forKey:identity.gaiaID];
  }
  _identityMap = mutableIdentityMap;

  [model addItem:[self addAccountItem]
      toSectionWithIdentifier:SectionIdentifierAccounts];

  // Sign out section.
  if (base::FeatureList::IsEnabled(kClearSyncedData)) {
    [model addSectionWithIdentifier:SectionIdentifierSignOut];
    // Adds a signout option if the account is not managed.
    if (![self authService]->IsAuthenticatedIdentityManaged()) {
      [model addItem:[self experimentalSignOutItem]
          toSectionWithIdentifier:SectionIdentifierSignOut];
    }
    // Adds a signout and clear data option.
    [model addItem:[self experimentalSignOutAndClearDataItem]
        toSectionWithIdentifier:SectionIdentifierSignOut];

    // Adds a footer with signout explanation depending on the type of
    // account whether managed or non-managed.
    if ([self authService]->IsAuthenticatedIdentityManaged()) {
      [model setFooter:[self signOutManagedAccountFooterItem]
          forSectionWithIdentifier:SectionIdentifierSignOut];
    } else {
      [model setFooter:[self signOutNonManagedAccountFooterItem]
          forSectionWithIdentifier:SectionIdentifierSignOut];
    }
  } else {
    [model addSectionWithIdentifier:SectionIdentifierSignOut];
    [model addItem:[self signOutItem]
        toSectionWithIdentifier:SectionIdentifierSignOut];
  }
}

#pragma mark - Model objects

- (TableViewTextHeaderFooterItem*)header {
  TableViewTextHeaderFooterItem* header =
      [[TableViewTextHeaderFooterItem alloc] initWithType:ItemTypeHeader];
  header.text = l10n_util::GetNSString(IDS_IOS_OPTIONS_ACCOUNTS_DESCRIPTION);
  return header;
}

- (TableViewLinkHeaderFooterItem*)signOutNonManagedAccountFooterItem {
  TableViewLinkHeaderFooterItem* footer = [[TableViewLinkHeaderFooterItem alloc]
      initWithType:ItemTypeSignOutNonManagedAccountFooter];
  footer.text = l10n_util::GetNSString(
      IDS_IOS_DISCONNECT_NON_MANAGED_ACCOUNT_FOOTER_INFO_MOBILE);
  return footer;
}

- (TableViewLinkHeaderFooterItem*)signOutManagedAccountFooterItem {
  TableViewLinkHeaderFooterItem* footer = [[TableViewLinkHeaderFooterItem alloc]
      initWithType:ItemTypeSignOutManagedAccountFooter];
  footer.text = l10n_util::GetNSStringF(
      IDS_IOS_DISCONNECT_MANAGED_ACCOUNT_FOOTER_INFO_MOBILE, self.hostedDomain);
  return footer;
}

- (TableViewItem*)accountItem:(ChromeIdentity*)identity {
  TableViewAccountItem* item =
      [[TableViewAccountItem alloc] initWithType:ItemTypeAccount];
  [self updateAccountItem:item withIdentity:identity];
  return item;
}

- (void)updateAccountItem:(TableViewAccountItem*)item
             withIdentity:(ChromeIdentity*)identity {
  item.image = [_avatarCache resizedAvatarForIdentity:identity];
  item.text = identity.userEmail;
  item.chromeIdentity = identity;
  item.accessibilityIdentifier = identity.userEmail;
  item.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
}

- (TableViewItem*)addAccountItem {
  TableViewAccountItem* item =
      [[TableViewAccountItem alloc] initWithType:ItemTypeAddAccount];
  item.text =
      l10n_util::GetNSString(IDS_IOS_OPTIONS_ACCOUNTS_ADD_ACCOUNT_BUTTON);
  item.accessibilityIdentifier = kSettingsAccountsTableViewAddAccountCellId;
  item.image = [[UIImage imageNamed:@"settings_accounts_add_account"]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  return item;
}

- (TableViewItem*)signOutItem {
  TableViewDetailTextItem* item =
      [[TableViewDetailTextItem alloc] initWithType:ItemTypeSignOut];
  item.text =
      l10n_util::GetNSString(IDS_IOS_OPTIONS_ACCOUNTS_SIGN_OUT_TURN_OFF_SYNC);
  item.accessibilityTraits |= UIAccessibilityTraitButton;
  item.accessibilityIdentifier = kSettingsAccountsTableViewSignoutCellId;
  return item;
}

- (TableViewItem*)experimentalSignOutItem {
  TableViewTextItem* item =
      [[TableViewTextItem alloc] initWithType:ItemTypeSignOut];
  item.text =
      l10n_util::GetNSString(IDS_IOS_DISCONNECT_DIALOG_CONTINUE_BUTTON_MOBILE);
  item.textColor = [UIColor colorNamed:kBlueColor];
  item.accessibilityTraits |= UIAccessibilityTraitButton;
  item.accessibilityIdentifier = kSettingsAccountsTableViewSignoutCellId;
  return item;
}

- (TableViewItem*)experimentalSignOutAndClearDataItem {
  TableViewTextItem* item =
      [[TableViewTextItem alloc] initWithType:ItemTypeSignOutAndClearData];
  item.text = l10n_util::GetNSString(
      IDS_IOS_DISCONNECT_DIALOG_CONTINUE_AND_CLEAR_MOBILE);
  item.textColor = [UIColor colorNamed:kRedColor];
  item.accessibilityTraits |= UIAccessibilityTraitButton;
  item.accessibilityIdentifier =
      kSettingsAccountsTableViewSignoutAndClearDataCellId;
  return item;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [super tableView:tableView didSelectRowAtIndexPath:indexPath];

  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];

  switch (itemType) {
    case ItemTypeAccount: {
      TableViewAccountItem* item =
          base::mac::ObjCCastStrict<TableViewAccountItem>(
              [self.tableViewModel itemAtIndexPath:indexPath]);
      DCHECK(item.chromeIdentity);
      UIView* itemView =
          [[tableView cellForRowAtIndexPath:indexPath] contentView];
      [self showAccountDetails:item.chromeIdentity itemView:itemView];
      break;
    }
    case ItemTypeAddAccount: {
      [self showAddAccount];
      break;
    }
    case ItemTypeSignOut: {
      if (base::FeatureList::IsEnabled(kClearSyncedData)) {
        UIView* itemView =
            [[tableView cellForRowAtIndexPath:indexPath] contentView];
        [self showSignOutWithClearData:NO itemView:itemView];
      } else {
        [self showSignOut];
      }
      break;
    }
    case ItemTypeSignOutAndClearData: {
      UIView* itemView =
          [[tableView cellForRowAtIndexPath:indexPath] contentView];
      [self showSignOutWithClearData:YES itemView:itemView];
      break;
    }
    default:
      break;
  }

  [self.tableView deselectRowAtIndexPath:indexPath animated:YES];
}

#pragma mark - IdentityManagerObserverBridgeDelegate

- (void)onEndBatchOfRefreshTokenStateChanges {
  [self reloadData];
  [self popViewIfSignedOut];
  if (![self authService] -> IsAuthenticated() &&
                                 _dimissAccountDetailsViewControllerBlock) {
    _dimissAccountDetailsViewControllerBlock(/*animated=*/YES);
    _dimissAccountDetailsViewControllerBlock = nil;
  }
}

#pragma mark - Authentication operations

- (void)showAddAccount {
  if ([_alertCoordinator isVisible])
    return;
  _authenticationOperationInProgress = YES;

  __weak __typeof(self) weakSelf = self;
  ShowSigninCommand* command = [[ShowSigninCommand alloc]
      initWithOperation:AUTHENTICATION_OPERATION_ADD_ACCOUNT
               identity:nil
            accessPoint:AccessPoint::ACCESS_POINT_SETTINGS
            promoAction:PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO
               callback:^(BOOL success) {
                 [weakSelf handleDidAddAccount:success];
               }];
  DCHECK(self.dispatcher);
  [self.dispatcher showSignin:command baseViewController:self];
}

- (void)handleDidAddAccount:(BOOL)success {
  [self handleAuthenticationOperationDidFinish];
  if (success && _closeSettingsOnAddAccount) {
    [self.dispatcher closeSettingsUI];
  }
}

- (void)showAccountDetails:(ChromeIdentity*)identity
                  itemView:(UIView*)itemView {
  if ([_alertCoordinator isVisible])
    return;
  if (base::FeatureList::IsEnabled(kEnableMyGoogle)) {
    _alertCoordinator = [[ActionSheetCoordinator alloc]
        initWithBaseViewController:self
                           browser:_browser
                             title:nil
                           message:identity.userEmail
                              rect:itemView.frame
                              view:itemView];

    [_alertCoordinator
        addItemWithTitle:l10n_util::GetNSString(
                             IDS_IOS_MANAGE_YOUR_GOOGLE_ACCOUNT_TITLE)
                  action:^{
                    _dimissAccountDetailsViewControllerBlock =
                        ios::GetChromeBrowserProvider()
                            ->GetChromeIdentityService()
                            ->PresentAccountDetailsController(identity, self,
                                                              /*animated=*/YES);
                  }
                   style:UIAlertActionStyleDefault];
    [_alertCoordinator addItemWithTitle:l10n_util::GetNSString(
                                            IDS_IOS_REMOVE_GOOGLE_ACCOUNT_TITLE)
                                 action:^{
                                   // TODO(crbug.com/1043080): Use Identity API
                                   // to remove account.
                                 }
                                  style:UIAlertActionStyleDestructive];

    [_alertCoordinator addItemWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                                 action:nil
                                  style:UIAlertActionStyleCancel];
    [_alertCoordinator start];
  } else {
    _dimissAccountDetailsViewControllerBlock =
        ios::GetChromeBrowserProvider()
            ->GetChromeIdentityService()
            ->PresentAccountDetailsController(identity, self, /*animated=*/YES);
  }
}

- (void)showSignOutWithClearData:(BOOL)forceClearData
                        itemView:(UIView*)itemView {
  NSString* alertMessage = nil;
  NSString* signOutTitle = nil;
  UIAlertActionStyle actionStyle = UIAlertActionStyleDefault;

  if (forceClearData) {
    alertMessage = l10n_util::GetNSString(
        IDS_IOS_DISCONNECT_DESTRUCTIVE_DIALOG_INFO_MOBILE);
    signOutTitle = l10n_util::GetNSString(
        IDS_IOS_DISCONNECT_DIALOG_CONTINUE_AND_CLEAR_MOBILE);
    actionStyle = UIAlertActionStyleDestructive;
  } else {
    alertMessage =
        l10n_util::GetNSString(IDS_IOS_DISCONNECT_KEEP_DATA_DIALOG_INFO_MOBILE);
    signOutTitle = l10n_util::GetNSString(
        IDS_IOS_DISCONNECT_DIALOG_CONTINUE_BUTTON_MOBILE);
    actionStyle = UIAlertActionStyleDefault;
  }

  _alertCoordinator =
      [[ActionSheetCoordinator alloc] initWithBaseViewController:self
                                                         browser:_browser
                                                           title:nil
                                                         message:alertMessage
                                                            rect:itemView.frame
                                                            view:itemView];

  __weak AccountsTableViewController* weakSelf = self;
  [_alertCoordinator
      addItemWithTitle:signOutTitle
                action:^{
                  [weakSelf handleSignOutWithForceClearData:forceClearData];
                }
                 style:actionStyle];
  [_alertCoordinator addItemWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                               action:nil
                                style:UIAlertActionStyleCancel];
  [_alertCoordinator start];
}

- (void)showSignOut {
  if (_authenticationOperationInProgress || [_alertCoordinator isVisible] ||
      self != [self.navigationController topViewController]) {
    // An action is already in progress, ignore user's request.
    return;
  }

  NSString* title = nil;
  NSString* message = nil;
  NSString* continueButtonTitle = nil;

  if ([self authService] -> IsAuthenticatedIdentityManaged()) {
    title =
        l10n_util::GetNSString(IDS_IOS_MANAGED_DISCONNECT_DIALOG_TITLE_UNITY);
    message = l10n_util::GetNSStringF(
        IDS_IOS_MANAGED_DISCONNECT_DIALOG_INFO_UNITY, self.hostedDomain);
    continueButtonTitle =
        l10n_util::GetNSString(IDS_IOS_MANAGED_DISCONNECT_DIALOG_ACCEPT_UNITY);
  } else {
    title = l10n_util::GetNSString(IDS_IOS_DISCONNECT_DIALOG_TITLE_UNITY);
    message =
        l10n_util::GetNSString(IDS_IOS_DISCONNECT_DIALOG_INFO_MOBILE_UNITY);
    continueButtonTitle = l10n_util::GetNSString(
        IDS_IOS_DISCONNECT_DIALOG_CONTINUE_BUTTON_MOBILE);
  }

  _alertCoordinator =
      [[AlertCoordinator alloc] initWithBaseViewController:self
                                                   browser:_browser
                                                     title:title
                                                   message:message];

  __weak AccountsTableViewController* weakSelf = self;
  [_alertCoordinator addItemWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                               action:nil
                                style:UIAlertActionStyleCancel];
  [_alertCoordinator addItemWithTitle:continueButtonTitle
                               action:^{
                                 [weakSelf handleSignOutWithForceClearData:NO];
                               }
                                style:UIAlertActionStyleDefault];

  [_alertCoordinator start];
}

- (void)handleSignOutWithForceClearData:(BOOL)forceClearData {
  AuthenticationService* authService = [self authService];
  if (authService->IsAuthenticated()) {
    _authenticationOperationInProgress = YES;
    [self preventUserInteraction];
    authService->SignOut(
        signin_metrics::USER_CLICKED_SIGNOUT_SETTINGS, forceClearData, ^{
          [self allowUserInteraction];
          _authenticationOperationInProgress = NO;
          [base::mac::ObjCCastStrict<SettingsNavigationController>(
              self.navigationController)
              popViewControllerOrCloseSettingsAnimated:YES];
        });
    // Get UMA metrics on the usage of the new UI, which is only available for
    // users in the experiement with non-managed accounts.
    if (base::FeatureList::IsEnabled(kClearSyncedData) &&
        ![self authService]->IsAuthenticatedIdentityManaged()) {
      UMA_HISTOGRAM_BOOLEAN("Signin.UserRequestedWipeDataOnSignout",
                            forceClearData);
    }
  }
}

// Sets |_authenticationOperationInProgress| to NO and pops this accounts
// table view controller if the user is signed out.
- (void)handleAuthenticationOperationDidFinish {
  DCHECK(_authenticationOperationInProgress);
  _authenticationOperationInProgress = NO;
  [self popViewIfSignedOut];
}

- (void)popViewIfSignedOut {
  if ([self authService] -> IsAuthenticated()) {
    return;
  }
  if (_authenticationOperationInProgress) {
    // The signed out state might be temporary (e.g. account switch, ...).
    // Don't pop this view based on intermediary values.
    return;
  }
  [self dismissSelfAnimated:NO];
}

- (void)dismissSelfAnimated:(BOOL)animated {
  if (_isBeingDismissed) {
    return;
  }
  _isBeingDismissed = YES;
  [_alertCoordinator stop];
  [self.navigationController popToViewController:self animated:NO];
  [base::mac::ObjCCastStrict<SettingsNavigationController>(
      self.navigationController)
      popViewControllerOrCloseSettingsAnimated:animated];
}

#pragma mark - Access to authentication service

- (AuthenticationService*)authService {
  return AuthenticationServiceFactory::GetForBrowserState(
      _browser->GetBrowserState());
}

#pragma mark - IdentityManager

- (base::string16)hostedDomain {
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForBrowserState(_browser->GetBrowserState());
  base::Optional<AccountInfo> accountInfo =
      identityManager->FindExtendedAccountInfoForAccountWithRefreshToken(
          identityManager->GetPrimaryAccountInfo());
  std::string hosted_domain = accountInfo.has_value()
                                  ? accountInfo.value().hosted_domain
                                  : std::string();
  return base::UTF8ToUTF16(hosted_domain);
}

#pragma mark - ChromeIdentityBrowserOpener

- (void)openURL:(NSURL*)url
              view:(UIView*)view
    viewController:(UIViewController*)viewController {
  OpenNewTabCommand* command =
      [OpenNewTabCommand commandWithURLFromChrome:net::GURLWithNSURL(url)];
  [self.dispatcher closeSettingsUIAndOpenURL:command];
}

#pragma mark - ChromeIdentityServiceObserver

- (void)profileUpdate:(ChromeIdentity*)identity {
  TableViewAccountItem* item = base::mac::ObjCCastStrict<TableViewAccountItem>(
      [_identityMap objectForKey:identity.gaiaID]);
  if (!item) {
    return;
  }
  [self updateAccountItem:item withIdentity:identity];
  NSIndexPath* indexPath = [self.tableViewModel indexPathForItem:item];
  [self.tableView reloadRowsAtIndexPaths:@[ indexPath ]
                        withRowAnimation:UITableViewRowAnimationAutomatic];
}

- (void)chromeIdentityServiceWillBeDestroyed {
  _identityServiceObserver.reset();
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  base::RecordAction(
      base::UserMetricsAction("IOSAccountsSettingsCloseWithSwipe"));
}

@end
