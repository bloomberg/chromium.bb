// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/elements/enterprise_info_popover_view_controller.h"

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/ui/settings/elements/elements_constants.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/popover_label_view_controller.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

NSString* const kEnterpriseIconName = @"enterprise_icon";

NSAttributedString* PrimaryMessage(NSString* fullText) {
  DCHECK(fullText);
  NSDictionary* generalAttributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextPrimaryColor],
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline]
  };

  return [[NSAttributedString alloc] initWithString:fullText
                                         attributes:generalAttributes];
}

NSAttributedString* SecondaryMessage(NSString* enterpriseName,
                                     BOOL addLearnMoreLink) {
  // Create and format the text.
  NSDictionary* textAttributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor],
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote]
  };

  NSString* message;
  NSAttributedString* attributedString;

  if (addLearnMoreLink) {
    if (enterpriseName) {
      message = l10n_util::GetNSStringF(
          IDS_IOS_ENTERPRISE_MANAGED_SETTING_DESC_WITH_COMPANY_NAME,
          base::SysNSStringToUTF16(enterpriseName));
    } else {
      message = l10n_util::GetNSString(
          IDS_IOS_ENTERPRISE_MANAGED_SETTING_DESC_WITHOUT_COMPANY_NAME);
    }

    NSDictionary* linkAttributes = @{
      NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor],
      NSFontAttributeName :
          [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote],
      NSLinkAttributeName :
          [NSString stringWithUTF8String:kChromeUIManagementURL],
    };

    // Add a space to have a distance with the leading icon.
    attributedString = AttributedStringFromStringWithLink(
        [@" " stringByAppendingString:message], textAttributes, linkAttributes);
  } else {
    message =
        l10n_util::GetNSString(IDS_IOS_ENTERPRISE_MANAGED_BY_YOUR_ORGANIZATION);

    // Add a space to have a distance with the leading icon.
    attributedString = [[NSAttributedString alloc]
        initWithString:[@" " stringByAppendingString:message]
            attributes:textAttributes];
  }

  // Create the leading enterprise icon.
  NSTextAttachment* attachment = [[NSTextAttachment alloc] init];
  attachment.image = [UIImage imageNamed:kEnterpriseIconName];
  NSAttributedString* attachmentString =
      [NSAttributedString attributedStringWithAttachment:attachment];

  // Making sure the image is well centered vertically relative to the text,
  // and also that the image scales with the text size.
  CGFloat height = attributedString.size.height;
  CGFloat capHeight =
      [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote].capHeight;
  CGFloat verticalOffset = roundf(capHeight - height) / 2.f;
  attachment.bounds = CGRectMake(0, verticalOffset, height, height);

  // Combine the icon and the text, and set them to the secondary label.
  NSMutableAttributedString* fullAtrributedString =
      [[NSMutableAttributedString alloc] initWithString:@""];
  [fullAtrributedString appendAttributedString:attachmentString];
  [fullAtrributedString appendAttributedString:attributedString];

  return fullAtrributedString;
}

}  // namespace

@interface EnterpriseInfoPopoverViewController ()

// YES if it is presented by a UIButton.
@property(nonatomic, assign) BOOL isPresentingFromButton;

@end

@implementation EnterpriseInfoPopoverViewController

- (instancetype)initWithEnterpriseName:(NSString*)enterpriseName {
  NSString* message =
      l10n_util::GetNSString(IDS_IOS_ENTERPRISE_MANAGED_SETTING_MESSAGE);
  return [self initWithMessage:message enterpriseName:enterpriseName];
}

- (instancetype)initWithMessage:(NSString*)message
                 enterpriseName:(NSString*)enterpriseName {
  return [self initWithMessage:message
                enterpriseName:enterpriseName
        isPresentingFromButton:YES
              addLearnMoreLink:YES];
}

- (instancetype)initWithMessage:(NSString*)message
                 enterpriseName:(NSString*)enterpriseName
         isPresentingFromButton:(BOOL)isPresentingFromButton
               addLearnMoreLink:(BOOL)addLearnMoreLink {
  self = [super
      initWithPrimaryAttributedString:PrimaryMessage(message)
            secondaryAttributedString:SecondaryMessage(enterpriseName,
                                                       addLearnMoreLink)];
  if (self) {
    _isPresentingFromButton = isPresentingFromButton;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.accessibilityIdentifier = kEnterpriseInfoBubbleViewId;
}

#pragma mark - UIPopoverPresentationControllerDelegate

- (void)popoverPresentationControllerDidDismissPopover:
    (UIPopoverPresentationController*)popoverPresentationController {
  if (self.isPresentingFromButton) {
    UIButton* buttonView = base::mac::ObjCCastStrict<UIButton>(
        popoverPresentationController.sourceView);
    buttonView.enabled = YES;
  }
}

@end
