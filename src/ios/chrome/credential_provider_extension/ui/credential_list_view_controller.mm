// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/credential_list_view_controller.h"

#include "base/mac/foundation_util.h"
#include "ios/chrome/common/app_group/app_group_metrics.h"
#import "ios/chrome/common/credential_provider/credential.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/highlight_button.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ios/chrome/credential_provider_extension/metrics_util.h"
#import "ios/chrome/credential_provider_extension/ui/credential_list_header_view.h"
#import "ios/chrome/credential_provider_extension/ui/feature_flags.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Reuse Identifiers for table views.
NSString* kHeaderIdentifier = @"clvcHeader";
NSString* kCredentialCellIdentifier = @"clvcCredentialCell";
NSString* kNewPasswordCellIdentifier = @"clvcNewPasswordCell";

const CGFloat kHeaderHeight = 70;
const CGFloat kNewCredentialHeaderHeight = 35;
// Add extra space to offset the top of the table view from the search bar.
const CGFloat kTableViewTopSpace = 8;

UIColor* BackgroundColor() {
  return IsPasswordCreationEnabled()
             ? [UIColor colorNamed:kGroupedPrimaryBackgroundColor]
             : [UIColor colorNamed:kBackgroundColor];
}
}

// This cell just adds a simple hover pointer interaction to the TableViewCell.
@interface CredentialListCell : UITableViewCell
@end

@implementation CredentialListCell

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    [self addInteraction:[[ViewPointerInteraction alloc] init]];
  }
  return self;
}

@end

@interface CredentialListViewController () <UITableViewDataSource,
                                            UISearchResultsUpdating>

// Search controller that contains search bar.
@property(nonatomic, strong) UISearchController* searchController;

// Current list of suggested passwords.
@property(nonatomic, copy) NSArray<id<Credential>>* suggestedPasswords;

// Current list of all passwords.
@property(nonatomic, copy) NSArray<id<Credential>>* allPasswords;

// Indicates if the option to create a new password should be presented.
@property(nonatomic, assign) BOOL showNewPasswordOption;

@end

@implementation CredentialListViewController

@synthesize delegate;

- (instancetype)init {
  UITableViewStyle style = IsPasswordCreationEnabled()
                               ? UITableViewStyleInsetGrouped
                               : UITableViewStylePlain;
  self = [super initWithStyle:style];
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title =
      NSLocalizedString(@"IDS_IOS_CREDENTIAL_PROVIDER_CREDENTIAL_LIST_TITLE",
                        @"AutoFill Chrome Password");
  self.view.backgroundColor = BackgroundColor();
  if (IsPasswordCreationEnabled()) {
    self.navigationItem.leftBarButtonItem = [self navigationCancelButton];
  } else {
    self.navigationItem.rightBarButtonItem = [self navigationCancelButton];
  }

  self.searchController =
      [[UISearchController alloc] initWithSearchResultsController:nil];
  self.searchController.searchResultsUpdater = self;
  self.searchController.obscuresBackgroundDuringPresentation = NO;
  self.searchController.searchBar.barTintColor = BackgroundColor();
  // Add en empty space at the bottom of the list, the size of the search bar,
  // to allow scrolling up enough to see last result, otherwise it remains
  // hidden under the accessories.
  self.tableView.tableFooterView =
      [[UIView alloc] initWithFrame:self.searchController.searchBar.frame];
  if (IsPasswordCreationEnabled()) {
    self.tableView.contentInset = UIEdgeInsetsMake(kTableViewTopSpace, 0, 0, 0);
  }
  self.navigationItem.searchController = self.searchController;
  self.navigationItem.hidesSearchBarWhenScrolling = NO;

  if (IsPasswordCreationEnabled()) {
    UINavigationBarAppearance* appearance =
        [[UINavigationBarAppearance alloc] init];
    [appearance configureWithDefaultBackground];
    appearance.backgroundColor = BackgroundColor();
    if (@available(iOS 15, *)) {
      self.navigationItem.scrollEdgeAppearance = appearance;
    } else {
      // On iOS 14, scrollEdgeAppearance only affects navigation bars with large
      // titles, so it can't be used. Instead, the navigation bar will always be
      // the same style.
      self.navigationItem.standardAppearance = appearance;
    }
  } else {
    self.navigationController.navigationBar.barTintColor = BackgroundColor();
    self.navigationController.navigationBar.shadowImage =
        [[UIImage alloc] init];
  }
  self.navigationController.navigationBar.tintColor =
      [UIColor colorNamed:kBlueColor];

  // Presentation of searchController will walk up the view controller hierarchy
  // until it finds the root view controller or one that defines a presentation
  // context. Make this class the presentation context so that the search
  // controller does not present on top of the navigation controller.
  self.definesPresentationContext = YES;
  [self.tableView registerClass:[UITableViewHeaderFooterView class]
      forHeaderFooterViewReuseIdentifier:kHeaderIdentifier];
  [self.tableView registerClass:[CredentialListHeaderView class]
      forHeaderFooterViewReuseIdentifier:CredentialListHeaderView.reuseID];
}

#pragma mark - CredentialListConsumer

- (void)presentSuggestedPasswords:(NSArray<id<Credential>>*)suggested
                     allPasswords:(NSArray<id<Credential>>*)all
                    showSearchBar:(BOOL)showSearchBar
            showNewPasswordOption:(BOOL)showNewPasswordOption {
  self.suggestedPasswords = suggested;
  self.allPasswords = all;
  self.showNewPasswordOption = showNewPasswordOption;
  [self.tableView reloadData];
  [self.tableView layoutIfNeeded];

  // Remove or add the search controller depending on whether there are
  // passwords to search.
  if (showSearchBar) {
    self.navigationItem.searchController = self.searchController;
  } else {
    if (self.navigationItem.searchController.isActive) {
      self.navigationItem.searchController.active = NO;
    }
    self.navigationItem.searchController = nil;
  }
}

- (void)setTopPrompt:(NSString*)prompt {
  self.navigationController.navigationBar.topItem.prompt = prompt;
}

#pragma mark - UITableViewDataSource

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView {
  return [self numberOfSections];
}

- (NSInteger)tableView:(UITableView*)tableView
    numberOfRowsInSection:(NSInteger)section {
  if ([self isEmptyTable]) {
    return 0;
  } else if ([self isSuggestedPasswordSection:section]) {
    return [self numberOfRowsInSuggestedPasswordSection];
  } else {
    return self.allPasswords.count;
  }
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  if ([self isIndexPathNewPasswordRow:indexPath]) {
    UITableViewCell* cell = [tableView
        dequeueReusableCellWithIdentifier:kNewPasswordCellIdentifier];
    if (!cell) {
      cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleSubtitle
                                    reuseIdentifier:kNewPasswordCellIdentifier];
    }
    cell.backgroundColor = [UIColor colorNamed:kBackgroundColor];
    cell.textLabel.text =
        NSLocalizedString(@"IDS_IOS_CREDENTIAL_PROVIDER_CREATE_PASSWORD_ROW",
                          @"Add New Password");
    cell.textLabel.textColor = [UIColor colorNamed:kBlueColor];
    return cell;
  }

  UITableViewCell* cell =
      [tableView dequeueReusableCellWithIdentifier:kCredentialCellIdentifier];
  if (!cell) {
    cell =
        [[CredentialListCell alloc] initWithStyle:UITableViewCellStyleSubtitle
                                  reuseIdentifier:kCredentialCellIdentifier];
    cell.accessoryView = [self infoIconButton];
  }

  id<Credential> credential = [self credentialForIndexPath:indexPath];
  cell.textLabel.text = credential.serviceName;
  cell.textLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  cell.detailTextLabel.text = credential.user;
  cell.detailTextLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  cell.selectionStyle = UITableViewCellSelectionStyleDefault;
  cell.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  cell.accessibilityTraits |= UIAccessibilityTraitButton;

  return cell;
}

#pragma mark - UITableViewDelegate

- (UIView*)tableView:(UITableView*)tableView
    viewForHeaderInSection:(NSInteger)section {
  if (IsPasswordCreationEnabled()) {
    CredentialListHeaderView* view = [self.tableView
        dequeueReusableHeaderFooterViewWithIdentifier:CredentialListHeaderView
                                                          .reuseID];
    view.headerTextLabel.text = [self titleForHeaderInSection:section];
    view.contentView.backgroundColor = BackgroundColor();
    return view;
  } else {
    UITableViewHeaderFooterView* view = [self.tableView
        dequeueReusableHeaderFooterViewWithIdentifier:kHeaderIdentifier];
    UIFontTextStyle textStyle = UIFontTextStyleCaption1;
    view.textLabel.text = [self titleForHeaderInSection:section];
    view.textLabel.font = [UIFont preferredFontForTextStyle:textStyle];
    view.contentView.backgroundColor = BackgroundColor();
    return view;
  }
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForHeaderInSection:(NSInteger)section {
  if (IsPasswordCreationEnabled() &&
      [self isSuggestedPasswordSection:section]) {
    return 0;
  }
  if (IsPasswordCreationEnabled()) {
    return kNewCredentialHeaderHeight;
  }
  return kHeaderHeight;
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  if ([self isIndexPathNewPasswordRow:indexPath]) {
    [self.delegate newPasswordWasSelected];
    [tableView deselectRowAtIndexPath:indexPath animated:YES];
    return;
  }
  UpdateUMACountForKey(app_group::kCredentialExtensionPasswordUseCount);
  id<Credential> credential = [self credentialForIndexPath:indexPath];
  [self.delegate userSelectedCredential:credential];
}

#pragma mark - UISearchResultsUpdating

- (void)updateSearchResultsForSearchController:
    (UISearchController*)searchController {
  if (searchController.searchBar.text.length) {
    UpdateUMACountForKey(app_group::kCredentialExtensionSearchCount);
  }
  [self.delegate updateResultsWithFilter:searchController.searchBar.text];
}

#pragma mark - Private

// Creates a cancel button for the navigation item.
- (UIBarButtonItem*)navigationCancelButton {
  UIBarButtonItem* cancelButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self.delegate
                           action:@selector(navigationCancelButtonWasPressed:)];
  cancelButton.tintColor = [UIColor colorNamed:kBlueColor];
  return cancelButton;
}

// Creates a button to be displayed as accessory of the password row item.
- (UIView*)infoIconButton {
  UIImage* image = [UIImage imageNamed:@"info_icon"];
  image = [image imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];

  HighlightButton* button = [HighlightButton buttonWithType:UIButtonTypeCustom];
  button.frame = CGRectMake(0.0, 0.0, image.size.width, image.size.height);
  [button setBackgroundImage:image forState:UIControlStateNormal];
  [button setTintColor:[UIColor colorNamed:kBlueColor]];
  [button addTarget:self
                action:@selector(infoIconButtonTapped:event:)
      forControlEvents:UIControlEventTouchUpInside];
  button.accessibilityLabel = NSLocalizedString(
      @"IDS_IOS_CREDENTIAL_PROVIDER_SHOW_DETAILS_ACCESSIBILITY_LABEL",
      @"Show Details.");

  button.pointerInteractionEnabled = YES;
  button.pointerStyleProvider = ^UIPointerStyle*(
      UIButton* theButton, __unused UIPointerEffect* proposedEffect,
      __unused UIPointerShape* proposedShape) {
    UITargetedPreview* preview =
        [[UITargetedPreview alloc] initWithView:theButton];
    UIPointerHighlightEffect* effect =
        [UIPointerHighlightEffect effectWithPreview:preview];
    UIPointerShape* shape =
        [UIPointerShape shapeWithRoundedRect:theButton.frame
                                cornerRadius:theButton.frame.size.width / 2];
    return [UIPointerStyle styleWithEffect:effect shape:shape];
  };

  return button;
}

// Called when info icon is tapped.
- (void)infoIconButtonTapped:(id)sender event:(id)event {
  CGPoint hitPoint =
      [base::mac::ObjCCastStrict<UIButton>(sender) convertPoint:CGPointZero
                                                         toView:self.tableView];
  NSIndexPath* indexPath = [self.tableView indexPathForRowAtPoint:hitPoint];
  id<Credential> credential = [self credentialForIndexPath:indexPath];
  [self.delegate showDetailsForCredential:credential];
}

// Returns number of sections to display based on |suggestedPasswords| and
// |allPasswords|. If no sections with data, returns 1 for the 'no data' banner.
- (int)numberOfSections {
  if ([self numberOfRowsInSuggestedPasswordSection] == 0 ||
      [self.allPasswords count] == 0) {
    return 1;
  }
  return 2;
}

// Returns YES if there is no data to display.
- (BOOL)isEmptyTable {
  return [self numberOfRowsInSuggestedPasswordSection] == 0 &&
         [self.allPasswords count] == 0;
}

// Returns YES if given section is for suggested passwords.
- (BOOL)isSuggestedPasswordSection:(int)section {
  int sections = [self numberOfSections];
  if ((sections == 2 && section == 0) ||
      (sections == 1 && [self numberOfRowsInSuggestedPasswordSection])) {
    return YES;
  } else {
    return NO;
  }
}

// Returns the credential at the passed index.
- (id<Credential>)credentialForIndexPath:(NSIndexPath*)indexPath {
  if ([self isSuggestedPasswordSection:indexPath.section]) {
    return self.suggestedPasswords[indexPath.row];
  } else {
    return self.allPasswords[indexPath.row];
  }
}

// Returns true if the passed index corresponds to the Create New Password Cell.
- (BOOL)isIndexPathNewPasswordRow:(NSIndexPath*)indexPath {
  if ([self isSuggestedPasswordSection:indexPath.section]) {
    return indexPath.row == NSInteger(self.suggestedPasswords.count);
  }
  return NO;
}

// Returns the number of rows in suggested passwords section.
- (NSUInteger)numberOfRowsInSuggestedPasswordSection {
  return [self.suggestedPasswords count] + (self.showNewPasswordOption ? 1 : 0);
}

// Returns the title of the given section
- (NSString*)titleForHeaderInSection:(NSInteger)section {
  if ([self isEmptyTable]) {
    return NSLocalizedString(@"IDS_IOS_CREDENTIAL_PROVIDER_NO_SEARCH_RESULTS",
                             @"No search results found");
  } else if ([self isSuggestedPasswordSection:section]) {
    if (IsPasswordCreationEnabled()) {
      return nil;
    }
    if (self.suggestedPasswords.count > 1) {
      return NSLocalizedString(
          @"IDS_IOS_CREDENTIAL_PROVIDER_SUGGESTED_PASSWORDS",
          @"Suggested Passwords");
    } else {
      return NSLocalizedString(
          @"IDS_IOS_CREDENTIAL_PROVIDER_SUGGESTED_PASSWORD",
          @"Suggested Password");
    }
  } else {
    return NSLocalizedString(@"IDS_IOS_CREDENTIAL_PROVIDER_ALL_PASSWORDS",
                             @"All Passwords");
  }
}

@end
