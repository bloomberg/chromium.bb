// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/app_list/cocoa/signin_view_controller.h"

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "skia/ext/skia_utils_mac.h"
#import "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/signin_delegate.h"
#import "ui/base/cocoa/controls/blue_label_button.h"
#import "ui/base/cocoa/controls/hyperlink_button_cell.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

const CGFloat kTopPadding = 40;
const CGFloat kBottomPadding = 40;
const CGFloat kLeftPadding = 40;
const CGFloat kRightPadding = 40;
const CGFloat kHeadingPadding = 30;
const CGFloat kButtonPadding = 40;

const CGFloat kTitleFontSizeDelta = 5;
const SkColor kLinkColor = SkColorSetRGB(0x11, 0x55, 0xcc);
const SkColor kTextColor = SkColorSetRGB(0x33, 0x33, 0x33);

// Padding on the left of the text in the NSTextField and HyperlinkButtonCell.
const CGFloat kTextFieldPadding = 2;

}  // namespace

@interface SigninViewController ()

- (void)onLearnMoreButtonClick:(id)sender;
- (void)onSettingsButtonClick:(id)sender;
- (void)onSigninButtonClick:(id)sender;
- (NSButton*)makeLinkButtonWithTitle:(NSString*)title
                              origin:(NSPoint)origin
                              action:(SEL)action;
- (NSTextField*)makeTextFieldWithText:(NSString*)text
                                 font:(NSFont*)font
                                frame:(NSRect)frame;
- (void)populateAndLayoutView;

@end

@interface AppsSigninView : NSView {
 @private
  CGFloat cornerRadius_;
}

@property(assign, nonatomic) CGFloat cornerRadius;

@end

@implementation SigninViewController;

- (id)initWithFrame:(NSRect)frame
       cornerRadius:(CGFloat)cornerRadius
           delegate:(app_list::SigninDelegate*)delegate {
  if ((self = [super init])) {
    base::scoped_nsobject<AppsSigninView> appsSigninView(
        [[AppsSigninView alloc] initWithFrame:frame]);
    [appsSigninView setCornerRadius:cornerRadius];
    delegate_ = delegate;
    [self setView:appsSigninView];
    [self populateAndLayoutView];
  }
  return self;
}

- (void)onLearnMoreButtonClick:(id)sender {
  delegate_->OpenLearnMore();
}

- (void)onSettingsButtonClick:(id)sender {
  delegate_->OpenSettings();
}

- (void)onSigninButtonClick:(id)sender {
  delegate_->ShowSignin();
}

- (NSButton*)makeLinkButtonWithTitle:(NSString*)title
                              origin:(NSPoint)origin
                              action:(SEL)action {
  base::scoped_nsobject<NSButton> button(
      [[HyperlinkButtonCell buttonWithString:title] retain]);
  [[button cell] setShouldUnderline:NO];
  [[button cell] setTextColor:gfx::SkColorToSRGBNSColor(kLinkColor)];
  [button sizeToFit];
  origin.x -= kTextFieldPadding;
  [button setFrameOrigin:origin];
  [button setTarget:self];
  [button setAction:action];
  return button.autorelease();
}

- (NSTextField*)makeTextFieldWithText:(NSString*)text
                                 font:(NSFont*)font
                                frame:(NSRect)frame {
  base::scoped_nsobject<NSTextField> textField(
      [[NSTextField alloc] initWithFrame:frame]);
  [textField setEditable:NO];
  [textField setSelectable:NO];
  [textField setDrawsBackground:NO];
  [textField setBezeled:NO];

  NSDictionary* textAttributes = @{
    NSFontAttributeName : font,
    NSForegroundColorAttributeName : gfx::SkColorToSRGBNSColor(kTextColor)
  };
  base::scoped_nsobject<NSAttributedString> attributedText(
      [[NSAttributedString alloc] initWithString:text
                                      attributes:textAttributes]);
  [textField setAttributedStringValue:attributedText];
  [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:textField];
  NSPoint origin = [textField frame].origin;
  origin.x -= kTextFieldPadding;
  origin.y -= NSHeight([textField bounds]);
  [textField setFrameOrigin:origin];
  return textField.autorelease();
}

- (void)populateAndLayoutView {
  NSView* signinView = [self view];
  NSRect frame = [signinView frame];
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  gfx::Font baseFont = rb.GetFont(ui::ResourceBundle::BaseFont);

  NSString* titleText = base::SysUTF16ToNSString(delegate_->GetSigninHeading());
  NSFont* titleFont = baseFont.DeriveFont(kTitleFontSizeDelta).GetNativeFont();
  NSRect rect = NSMakeRect(kLeftPadding, NSHeight(frame) - kTopPadding,
                           NSWidth(frame) - kLeftPadding - kRightPadding, 0);
  NSTextField* titleTextView = [self makeTextFieldWithText:titleText
                                                      font:titleFont
                                                     frame:rect];
  [signinView addSubview:titleTextView];

  NSString* signinText = base::SysUTF16ToNSString(delegate_->GetSigninText());
  rect.origin.y = floor(
      NSMinY([titleTextView frame]) + [titleFont descender] - kHeadingPadding);
  NSFont* signinTextFont = baseFont.GetNativeFont();
  NSTextField* signinTextView = [self makeTextFieldWithText:signinText
                                                       font:signinTextFont
                                                      frame:rect];
  [signinView addSubview:signinTextView];

  base::scoped_nsobject<BlueLabelButton> button(
      [[BlueLabelButton alloc] initWithFrame:NSZeroRect]);
  [button setTitle:base::SysUTF16ToNSString(delegate_->GetSigninButtonText())];
  [button setKeyEquivalent:@"\r"];
  [button setTarget:self];
  [button setAction:@selector(onSigninButtonClick:)];
  [button sizeToFit];
  NSPoint buttonOrigin = NSMakePoint(kLeftPadding, floor(
      NSMinY([signinTextView frame]) - [signinTextFont descender] -
      NSHeight([button frame]) - kButtonPadding));
  [button setFrameOrigin:buttonOrigin];
  [signinView addSubview:button];

  NSString* settingsLinkText =
      base::SysUTF16ToNSString(delegate_->GetSettingsLinkText());
  NSButton* settingsLink =
      [self makeLinkButtonWithTitle:settingsLinkText
                             origin:NSMakePoint(kLeftPadding, kBottomPadding)
                             action:@selector(onSettingsButtonClick:)];
  NSPoint origin = NSMakePoint(kLeftPadding,
                               NSMaxY([settingsLink frame]));
  NSString* learnMoreLinkText =
      base::SysUTF16ToNSString(delegate_->GetLearnMoreLinkText());
  NSButton* learnMoreLink =
      [self makeLinkButtonWithTitle:learnMoreLinkText
                             origin:origin
                             action:@selector(onLearnMoreButtonClick:)];
  [signinView addSubview:learnMoreLink];
  [signinView addSubview:settingsLink];
}

@end

@implementation AppsSigninView

@synthesize cornerRadius = cornerRadius_;

- (void)drawRect:(NSRect)dirtyRect {
  [gfx::SkColorToSRGBNSColor(app_list::kContentsBackgroundColor) set];
  [[NSBezierPath bezierPathWithRoundedRect:[self bounds]
                                   xRadius:cornerRadius_
                                   yRadius:cornerRadius_] fill];
}

@end
