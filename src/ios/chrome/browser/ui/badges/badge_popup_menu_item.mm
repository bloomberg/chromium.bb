// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/badges/badge_popup_menu_item.h"

#include <ostream>

#import "base/notreached.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/icons/chrome_symbol.h"
#import "ios/chrome/browser/ui/icons/infobar_icon.h"
#import "ios/chrome/browser/ui/list_model/list_model.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Height of the ContentView of the cell.
const CGFloat kCellHeight = 44;
// Horizontal spacing between subviews.
const CGFloat kMargin = 15;
// Maximum of the cell.
const CGFloat kMaxHeight = 100;
// Vertical spacing between subviews.
const CGFloat kVerticalMargin = 8;
// Corner radius of badge background.
const CGFloat kBadgeCornerRadius = 5.0;
}  // namespace

@interface BadgePopupMenuItem ()

// The title of the item.
@property(nonatomic, copy, readonly) NSString* title;

// The BadgeType of the item.
@property(nonatomic, assign, readonly) BadgeType badgeType;

@end

@implementation BadgePopupMenuItem
// Synthesized from BadgeItem.
@synthesize actionIdentifier = _actionIdentifier;

- (instancetype)initWithBadgeType:(BadgeType)badgeType {
  self = [super initWithType:kItemTypeEnumZero];
  if (self) {
    self.cellClass = [BadgePopupMenuCell class];
    _badgeType = badgeType;
    switch (badgeType) {
      case kBadgeTypePasswordSave:
        _actionIdentifier = PopupMenuActionShowSavePasswordOptions;
        _title = l10n_util::GetNSString(
            IDS_IOS_PASSWORD_MANAGER_SAVE_PASSWORD_TITLE);
        break;
      case kBadgeTypePasswordUpdate:
        _actionIdentifier = PopupMenuActionShowUpdatePasswordOptions;
        _title = l10n_util::GetNSString(
            IDS_IOS_PASSWORD_MANAGER_UPDATE_PASSWORD_TITLE);
        break;
      case kBadgeTypeSaveAddressProfile:
        _actionIdentifier = PopupMenuActionShowSaveAddressProfileOptions;
        _title =
            l10n_util::GetNSString(IDS_IOS_AUTOFILL_SAVE_ADDRESS_PROMPT_TITLE);
        break;
      case kBadgeTypeSaveCard:
        _actionIdentifier = PopupMenuActionShowSaveCardOptions;
        _title = l10n_util::GetNSString(IDS_IOS_AUTOFILL_SAVE_CARD);
        break;
      case kBadgeTypeTranslate:
        _actionIdentifier = PopupMenuActionShowTranslateOptions;
        _title = l10n_util::GetNSString(IDS_IOS_TRANSLATE_INFOBAR_MODAL_TITLE);
        break;
      case kBadgeTypeAddToReadingList:
        _actionIdentifier = PopupMenuActionAddToReadingListOptions;
        _title =
            l10n_util::GetNSString(IDS_IOS_READING_LIST_MESSAGES_MODAL_TITLE);
        break;
      case kBadgeTypePermissionsCamera:
        // Falls through.
      case kBadgeTypePermissionsMicrophone:
        _actionIdentifier = PopupMenuActionShowPermissionsOptions;
        _title = l10n_util::GetNSString(
            IDS_IOS_PERMISSIONS_INFOBAR_OVERFLOW_POPUP_TITLE);
        break;
      case kBadgeTypeIncognito:
        NOTREACHED() << "A BadgePopupMenuItem should not be an Incognito badge";
        break;
      case kBadgeTypeOverflow:
        NOTREACHED() << "A BadgePopupMenuItem should not be an overflow badge";
        break;
      case kBadgeTypeNone:
        NOTREACHED() << "A badge should not have kBadgeTypeNone";
        break;
    }
  }
  return self;
}

- (void)configureCell:(BadgePopupMenuCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  cell.titleLabel.text = self.title;
  cell.accessibilityTraits = UIAccessibilityTraitButton;
  UIImage* badgeImage;
  NSString* imageName =
      base::FeatureList::IsEnabled(
          password_manager::features::kIOSEnablePasswordManagerBrandingUpdate)
          ? @"password_key"
          : @"legacy_password_key";
  switch (self.badgeType) {
    case kBadgeTypePasswordSave:
    case kBadgeTypePasswordUpdate:
      badgeImage = [[UIImage imageNamed:imageName]
          imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
      break;
    case kBadgeTypeSaveAddressProfile:
      badgeImage = UseSymbols() ? DefaultSymbolWithPointSize(
                                      kPinFillSymbol, kSymbolImagePointSize)
                                : [UIImage imageNamed:@"ic_place"];
      badgeImage = [badgeImage
          imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
      break;
    case kBadgeTypeSaveCard:
      badgeImage = UseSymbols()
                       ? DefaultSymbolWithPointSize(kCreditCardSymbol,
                                                    kSymbolImagePointSize)
                       : [UIImage imageNamed:@"infobar_save_card_icon"];
      badgeImage = [badgeImage
          imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
      break;

    case kBadgeTypeTranslate:
      badgeImage = UseSymbols()
                       ? CustomSymbolWithPointSize(kTranslateSymbol,
                                                   kSymbolImagePointSize)
                       : [UIImage imageNamed:@"infobar_translate_icon"];
      badgeImage = [badgeImage
          imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
      break;
    case kBadgeTypeAddToReadingList:
      badgeImage = [[UIImage imageNamed:@"infobar_reading_list"]
          imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
      break;
    case kBadgeTypePermissionsCamera:
      badgeImage = CustomSymbolTemplateWithPointSize(kCameraSymbol,
                                                     kSymbolImagePointSize);
      break;
    case kBadgeTypePermissionsMicrophone:
      badgeImage = DefaultSymbolTemplateWithPointSize(kMicrophoneSymbol,
                                                      kSymbolImagePointSize);
      break;
    case kBadgeTypeIncognito:
      NOTREACHED()
          << "A popup menu item should not be of type kBadgeTypeIncognito";
      break;
    case kBadgeTypeOverflow:
      NOTREACHED()
          << "A popup menu item should not be of type kBadgeTypeOverflow";
      break;
    case kBadgeTypeNone:
      NOTREACHED() << "A badge should not have kBadgeTypeNone";
  }
  [cell setBadgeImage:badgeImage];
}

#pragma mark - PopupMenuItem

- (CGSize)cellSizeForWidth:(CGFloat)width {
  // TODO(crbug.com/828357): This should be done at the table view level.
  static BadgePopupMenuCell* cell;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    cell = [[BadgePopupMenuCell alloc] init];
  });

  [self configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];
  cell.frame = CGRectMake(0, 0, width, kMaxHeight);
  [cell setNeedsLayout];
  [cell layoutIfNeeded];
  return [cell systemLayoutSizeFittingSize:CGSizeMake(width, kMaxHeight)];
}

@end

@interface BadgePopupMenuCell ()

// The font to be used for the title.
@property(nonatomic, strong, readonly) UIFont* titleFont;

// Image view to display the badge.
@property(nonatomic, strong, readonly) UIImageView* badgeView;

// Image view displayed in the trailing corner of the cell.
@property(nonatomic, strong, readonly) UIImageView* trailingImageView;

@end

@implementation BadgePopupMenuCell

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    _titleFont = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _titleLabel = [[UILabel alloc] init];
    _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _titleLabel.numberOfLines = 0;
    _titleLabel.lineBreakMode = NSLineBreakByWordWrapping;
    _titleLabel.font = _titleFont;
    _titleLabel.textColor = [UIColor colorNamed:kBlueColor];
    _titleLabel.adjustsFontForContentSizeCategory = YES;

    // ImageView is wrapped inside a wrapper to support symbol image. Doing so
    // makes sure that the size of the badge stays the same across all cells,
    // regardless of the badge image being used.
    _badgeView = [[UIImageView alloc] init];
    _badgeView.translatesAutoresizingMaskIntoConstraints = NO;
    UIView* badgeWrapperView = [[UIView alloc] init];
    badgeWrapperView.translatesAutoresizingMaskIntoConstraints = NO;
    badgeWrapperView.tintColor = [UIColor colorNamed:kBlueColor];
    badgeWrapperView.backgroundColor = [UIColor colorNamed:kBlueHaloColor];
    badgeWrapperView.layer.cornerRadius = kBadgeCornerRadius;
    [badgeWrapperView addSubview:_badgeView];

    [NSLayoutConstraint activateConstraints:@[
      [badgeWrapperView.widthAnchor
          constraintEqualToConstant:kCellHeight - 2 * kVerticalMargin],
      [badgeWrapperView.centerXAnchor
          constraintEqualToAnchor:_badgeView.centerXAnchor],
      [badgeWrapperView.centerYAnchor
          constraintEqualToAnchor:_badgeView.centerYAnchor],
    ]];

    _trailingImageView = [[UIImageView alloc]
        initWithImage:
            [[UIImage imageNamed:@"infobar_settings_icon"]
                imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate]];
    _trailingImageView.translatesAutoresizingMaskIntoConstraints = NO;
    _trailingImageView.tintColor = [UIColor colorNamed:kBlueColor];

    [self.contentView addSubview:_titleLabel];
    [self.contentView addSubview:badgeWrapperView];
    [self.contentView addSubview:_trailingImageView];

    ApplyVisualConstraintsWithMetrics(
        @[
          @"H:|-(margin)-[badge]-(margin)-[text]-(margin)-[gear]-(margin)-|",
          @"V:|-(verticalMargin)-[text]-(verticalMargin)-|",
          @"V:|-(verticalMargin)-[badge]-(verticalMargin)-|",
          @"V:|-(verticalMargin)-[gear]-(verticalMargin)-|",
        ],
        @{
          @"text" : _titleLabel,
          @"badge" : badgeWrapperView,
          @"gear" : _trailingImageView,
        },
        @{
          @"margin" : @(kMargin),
          @"verticalMargin" : @(kVerticalMargin),
        });

    [self.contentView.heightAnchor
        constraintGreaterThanOrEqualToConstant:kCellHeight]
        .active = YES;

    self.isAccessibilityElement = YES;
  }
  return self;
}

- (void)setBadgeImage:(UIImage*)badgeImage {
  [self.badgeView setImage:badgeImage];
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.titleLabel.text = @"";
  [self.badgeView setImage:nil];
  [self.trailingImageView setImage:nil];
}

@end
