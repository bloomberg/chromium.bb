// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/empty_reading_list_message_util.h"

#include "base/logging.h"
#include "ios/chrome/browser/experimental_flags.h"
#include "ios/chrome/browser/ui/rtl_geometry.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Images name.
NSString* const kToolsIcon = @"reading_list_tools_icon";
NSString* const kToolsIconLegacy = @"reading_list_toolbar_icon";
NSString* const kShareIconLegacy = @"reading_list_share_icon";

// Tag in string.
NSString* const kOpenShareMarker = @"SHARE_OPENING_ICON";
NSString* const kReadLaterTextMarker = @"READ_LATER_TEXT";

// Background view constants.
const CGFloat kFontSize = 16;
const CGFloat kLineHeight = 24;

// Enum type describing the icons used by the attributed empty table message.
enum class IconType { TOOLS, SHARE };
// Returns the UIImage corresponding with |icon_type|.
UIImage* GetIconWithType(IconType icon_type) {
  if (experimental_flags::IsReadingListUIRebootEnabled()) {
    switch (icon_type) {
      case IconType::TOOLS:
        return [UIImage imageNamed:kToolsIcon];
      case IconType::SHARE:
        NOTREACHED() << "The share icon is not used in the UI refresh.";
        return nil;
    }
  } else {
    switch (icon_type) {
      case IconType::TOOLS:
        return [UIImage imageNamed:kToolsIconLegacy];
      case IconType::SHARE:
        return [UIImage imageNamed:kShareIconLegacy];
    }
  }
}

// Returns the font to use for the message text.
UIFont* GetMessageFont() {
  return experimental_flags::IsReadingListUIRebootEnabled()
             ? [UIFont preferredFontForTextStyle:UIFontTextStyleBody]
             : [[MDCTypography fontLoader] regularFontOfSize:kFontSize];
}

// Returns the attributes to use for the message text.
NSMutableDictionary* GetMessageAttributes() {
  NSMutableDictionary* attributes = [NSMutableDictionary dictionary];
  UIFont* font = GetMessageFont();
  attributes[NSFontAttributeName] = font;
  attributes[NSForegroundColorAttributeName] =
      experimental_flags::IsReadingListUIRebootEnabled()
          ? [UIColor grayColor]
          : [[MDCPalette greyPalette] tint700];
  NSMutableParagraphStyle* paragraph_style =
      [[NSMutableParagraphStyle alloc] init];
  paragraph_style.lineBreakMode = NSLineBreakByWordWrapping;
  paragraph_style.alignment = NSTextAlignmentCenter;
  // If the line wrapping occurs that one of the icons is the first character on
  // a new line, the default line spacing will result in uneven line heights.
  // Manually setting the line spacing here prevents that from occurring.
  paragraph_style.lineSpacing = kLineHeight - font.lineHeight;
  attributes[NSParagraphStyleAttributeName] = paragraph_style;
  return attributes;
}

// Returns the attributes to use for the instructions on how to reach the "Read
// Later" option.
NSMutableDictionary* GetInstructionAttributes() {
  NSMutableDictionary* attributes = GetMessageAttributes();
  attributes[NSFontAttributeName] =
      experimental_flags::IsReadingListUIRebootEnabled()
          ? [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline]
          : [[MDCTypography fontLoader] boldFontOfSize:kFontSize];
  return attributes;
}

// Returns the "Read Later" text to appear at the end of the string, with
// correct styling.
NSAttributedString* GetReadLaterString() {
  NSString* read_later_text =
      l10n_util::GetNSString(experimental_flags::IsReadingListUIRebootEnabled()
                                 ? IDS_IOS_SHARE_MENU_READING_LIST_ACTION
                                 : IDS_IOS_CONTENT_CONTEXT_ADDTOREADINGLIST);
  return [[NSAttributedString alloc] initWithString:read_later_text
                                         attributes:GetInstructionAttributes()];
}

// Appends the icon with |icon_type| to |text|.  Spacer text that is added by
// this function is formatted with |attributes|.
void AppendIcon(IconType icon_type,
                NSMutableAttributedString* text,
                NSDictionary* attributes) {
  // Add a zero width space to set the attributes for the image.
  NSAttributedString* spacer =
      [[NSAttributedString alloc] initWithString:@"\u200B"
                                      attributes:attributes];
  [text appendAttributedString:spacer];

  // The icon bounds must be offset to be vertically centered with the message
  // text.
  UIImage* icon = GetIconWithType(icon_type);
  CGRect icon_bounds = CGRectZero;
  icon_bounds.size = icon.size;
  icon_bounds.origin.y = (GetMessageFont().xHeight - icon.size.height) / 2.0;

  // Attach the icon image.
  NSTextAttachment* attachment = [[NSTextAttachment alloc] init];
  attachment.image =
      [icon imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  attachment.bounds = icon_bounds;
  NSAttributedString* attachment_string =
      [NSAttributedString attributedStringWithAttachment:attachment];
  [text appendAttributedString:attachment_string];
}

// Appends a carat string and some spacing to |text|.
void AppendCarat(NSMutableAttributedString* text, NSDictionary* attributes) {
  // Use a carat facing the appropriate direction for the language.
  NSString* carat = [NSString
      stringWithFormat:@" %@ ", UseRTLLayout() ? @"\u25C2" : @"\u25B8"];
  [text appendAttributedString:[[NSAttributedString alloc]
                                   initWithString:carat
                                       attributes:attributes]];
}

// Returns the string to use to describe the buttons needed to access the "Read
// Later" option.
NSAttributedString* GetInstructionIconString() {
  NSDictionary* attributes = GetInstructionAttributes();
  NSMutableAttributedString* icon_string =
      [[NSMutableAttributedString alloc] init];
  if (experimental_flags::IsReadingListUIRebootEnabled()) {
    // In the UI reboot, only the single tools icon is used.
    AppendIcon(IconType::TOOLS, icon_string, attributes);
  } else {
    if (IsCompactWidth() || !IsIPadIdiom()) {
      // TODO(crbug.com/698726): When the share icon is displayed in the toolbar
      // for landscape iPhone 6+, remove !IsIPadIdiom().
      // If the device has a compact display the share menu is accessed from the
      // toolbar menu. If it is expanded, the share menu is directly accessible.
      AppendIcon(IconType::TOOLS, icon_string, attributes);
      AppendCarat(icon_string, attributes);
    }
    AppendIcon(IconType::SHARE, icon_string, attributes);
    AppendCarat(icon_string, attributes);
    // Append an additional space at the end of the legacy icon string to
    // improve the kerning between the carat and the "Read Later" text.
    [icon_string appendAttributedString:[[NSAttributedString alloc]
                                            initWithString:@" "
                                                attributes:attributes]];
  }
  return icon_string;
}

// Returns the icon string in an accessible format.
NSAttributedString* GetAccessibleInstructionIconString() {
  NSDictionary* attributes = GetInstructionAttributes();
  NSMutableAttributedString* icon_string =
      [[NSMutableAttributedString alloc] initWithString:@":"
                                             attributes:attributes];
  if (experimental_flags::IsReadingListUIRebootEnabled()) {
    NSString* tools_text = [NSString
        stringWithFormat:@"%@, ",
                         l10n_util::GetNSString(IDS_IOS_TOOLBAR_SETTINGS)];
    [icon_string appendAttributedString:[[NSAttributedString alloc]
                                            initWithString:tools_text
                                                attributes:attributes]];
  } else {
    if ((IsCompactWidth() || !IsIPadIdiom())) {
      NSString* tools_text = [NSString
          stringWithFormat:@"%@, ",
                           l10n_util::GetNSString(IDS_IOS_TOOLBAR_SETTINGS)];
      [icon_string appendAttributedString:[[NSAttributedString alloc]
                                              initWithString:tools_text
                                                  attributes:attributes]];
    }
    NSString* share_text = [NSString
        stringWithFormat:@"%@, ",
                         l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_SHARE)];
    [icon_string appendAttributedString:[[NSAttributedString alloc]
                                            initWithString:share_text
                                                attributes:attributes]];
  }
  return icon_string;
}

// Returns the attributed text to use for the message.  If |use_icons| is true,
// icon images are added to the text; otherwise accessible text versions of the
// instructions are used.
NSAttributedString* GetReadingListEmptyMessage(bool use_icons) {
  bool reboot_enabled = experimental_flags::IsReadingListUIRebootEnabled();
  NSString* raw_text = l10n_util::GetNSString(
      reboot_enabled ? IDS_IOS_READING_LIST_EMPTY_MESSAGE
                     : IDS_IOS_READING_LIST_EMPTY_MESSAGE_LEGACY);
  NSMutableAttributedString* message =
      [[NSMutableAttributedString alloc] initWithString:raw_text
                                             attributes:GetMessageAttributes()];
  NSAttributedString* instruction_icon_string =
      use_icons ? GetInstructionIconString()
                : GetAccessibleInstructionIconString();
  NSAttributedString* read_later_string = GetReadLaterString();
  if (reboot_enabled) {
    // When the reboot is enabled, two replacements must be made in the text:
    // - kOpenShareMarker should be replaced with |instruction_icon_string|
    // - kReadLaterTextMarker should be replaced with |read_later_text|
    NSRange icon_range = [message.string rangeOfString:kOpenShareMarker];
    DCHECK(icon_range.location != NSNotFound);
    [message replaceCharactersInRange:icon_range
                 withAttributedString:instruction_icon_string];

    NSRange read_later_range =
        [message.string rangeOfString:kReadLaterTextMarker];
    DCHECK(read_later_range.location != NSNotFound);
    [message replaceCharactersInRange:read_later_range
                 withAttributedString:read_later_string];
  } else {
    // In the legacy implementation, kOpenShareMarker is replaced with the
    // entire instruction string (i.e. "(tools icon) > (share icon) > Read
    // Later".
    NSMutableAttributedString* instruction_string =
        [[NSMutableAttributedString alloc] init];
    [instruction_string appendAttributedString:instruction_icon_string];
    [instruction_string appendAttributedString:read_later_string];
    NSRange replacement_range = [message.string rangeOfString:kOpenShareMarker];
    DCHECK(replacement_range.location != NSNotFound);
    [message replaceCharactersInRange:replacement_range
                 withAttributedString:instruction_string];
  }
  return message;
}
}  // namespace

NSAttributedString* GetReadingListEmptyMessage() {
  return GetReadingListEmptyMessage(true);
}

NSString* GetReadingListEmptyMessageA11yLabel() {
  return GetReadingListEmptyMessage(false).string;
}
