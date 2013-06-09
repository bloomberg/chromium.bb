// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/message_center/cocoa/settings_controller.h"

#include "base/strings/sys_string_conversions.h"
#include "grit/ui_resources.h"
#include "grit/ui_strings.h"
#import "ui/base/cocoa/hover_image_button.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#import "base/memory/scoped_nsobject.h"
#import "ui/message_center/cocoa/tray_view_controller.h"
#include "ui/message_center/message_center_style.h"
#include "skia/ext/skia_utils_mac.h"

const int kBackButtonSize = 32;
const int kMarginWidth = 16;
const int kEntryHeight = 32;

namespace message_center {

NotifierSettingsDelegateMac::~NotifierSettingsDelegateMac() {}

void NotifierSettingsDelegateMac::UpdateIconImage(const std::string& id,
                                                  const gfx::Image& icon) {
  // TODO(thakis): Implement.
}

void NotifierSettingsDelegateMac::UpdateFavicon(const GURL& url,
                                                const gfx::Image& icon) {
  // TODO(thakis): Implement.
}

NotifierSettingsDelegate* ShowSettings(NotifierSettingsProvider* provider,
                                       gfx::NativeView context) {
  // The caller of this function (the tray) retains |controller| while it's
  // visible.
  MCSettingsController* controller =
      [[[MCSettingsController alloc] initWithProvider:provider] autorelease];
  return [controller delegate];
}

}  // namespace message_center

@implementation MCSettingsController

- (id)initWithProvider:(message_center::NotifierSettingsProvider*)provider {
  if ((self = [super initWithNibName:nil bundle:nil])) {
    delegate_.reset(new message_center::NotifierSettingsDelegateMac(self));
    provider_ = provider;
  }
  return self;
}

- (void)dealloc {
  provider_->OnNotifierSettingsClosing();
  [super dealloc];
}

- (NSTextField*)newLabelWithFrame:(NSRect)frame {
  NSTextField* label = [[NSTextField alloc] initWithFrame:frame];
  [label setDrawsBackground:NO];
  [label setBezeled:NO];
  [label setEditable:NO];
  [label setSelectable:NO];
  [label setAutoresizingMask:NSViewMinYMargin];
  return label;
}

- (void)loadView {
  provider_->GetNotifierList(&notifiers_);
  CGFloat maxHeight = [MCTrayViewController maxTrayHeight];

  // Container view.
  NSRect fullFrame =
      NSMakeRect(0, 0, [MCTrayViewController trayWidth], maxHeight);
  scoped_nsobject<NSBox> view([[NSBox alloc] initWithFrame:fullFrame]);
  [view setBorderType:NSNoBorder];
  [view setBoxType:NSBoxCustom];
  [view setContentViewMargins:NSZeroSize];
  [view setFillColor:gfx::SkColorToCalibratedNSColor(
      message_center::kMessageCenterBackgroundColor)];
  [view setTitlePosition:NSNoTitle];
  [self setView:view];

  // Back button.
  NSRect backButtonFrame = NSMakeRect(
      kMarginWidth, kMarginWidth, kBackButtonSize, kBackButtonSize);
  backButton_.reset([[HoverImageButton alloc] initWithFrame:backButtonFrame]);
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  [backButton_ setDefaultImage:
      rb.GetNativeImageNamed(IDR_NOTIFICATION_ARROW).ToNSImage()];
  [backButton_ setHoverImage:
      rb.GetNativeImageNamed(IDR_NOTIFICATION_ARROW_HOVER).ToNSImage()];
  [backButton_ setPressedImage:
      rb.GetNativeImageNamed(IDR_NOTIFICATION_ARROW_PRESSED).ToNSImage()];
  [[backButton_ cell] setHighlightsBy:NSOnState];
  [backButton_ setBordered:NO];
  [backButton_ setAutoresizingMask:NSViewMinYMargin];
  [[self view] addSubview:backButton_];

  // "Settings" text.
  NSRect headerFrame = NSMakeRect(NSMaxX(backButtonFrame),
                                  kMarginWidth,
                                  NSWidth(fullFrame) - NSMaxX(backButtonFrame),
                                  NSHeight(fullFrame));
  settingsText_.reset([self newLabelWithFrame:headerFrame]);
  [settingsText_ setAutoresizingMask:NSViewMinYMargin];
  [settingsText_ setTextColor:gfx::SkColorToCalibratedNSColor(
      message_center::kRegularTextColor)];
  [settingsText_ setFont:
      [NSFont messageFontOfSize:message_center::kTitleFontSize]];

  [settingsText_ setStringValue:
      l10n_util::GetNSString(IDS_MESSAGE_CENTER_SETTINGS_BUTTON_LABEL)];
  [settingsText_ sizeToFit];
  headerFrame = [settingsText_ frame];
  headerFrame.origin.y =
      NSMaxY(fullFrame) - kMarginWidth - NSHeight(headerFrame);
  [[self view] addSubview:settingsText_];

  // Vertically center back button with "Settings" text.
  backButtonFrame.origin.y =
      NSMidY(headerFrame) - NSHeight(backButtonFrame) / 2;

  // Subheader.
  NSRect subheaderFrame = NSMakeRect(
      kMarginWidth, kMarginWidth, NSWidth(fullFrame), NSHeight(fullFrame));
  detailsText_.reset([self newLabelWithFrame:subheaderFrame]);
  [detailsText_ setAutoresizingMask:NSViewMinYMargin];
  [detailsText_ setTextColor:gfx::SkColorToCalibratedNSColor(
      message_center::kDimTextColor)];
  [detailsText_ setFont:
      [NSFont messageFontOfSize:message_center::kMessageFontSize]];

  [detailsText_ setStringValue:l10n_util::GetNSString(
      IDS_MESSAGE_CENTER_SETTINGS_DIALOG_DESCRIPTION)];
  [detailsText_ sizeToFit];
  subheaderFrame = [detailsText_ frame];
  subheaderFrame.origin.y =
      NSMinY(headerFrame) - message_center::kTextTopPadding -
      NSHeight(subheaderFrame);
  [[self view] addSubview:detailsText_];

  // Document view for the notifier settings.
  CGFloat y = 0;
  NSRect documentFrame = NSMakeRect(0, 0, NSWidth(fullFrame), 0);
  scoped_nsobject<NSView> documentView(
      [[NSView alloc] initWithFrame:documentFrame]);
  for (int i = notifiers_.size() - 1; i >= 0; --i) {
    message_center::Notifier* notifier = notifiers_[i];

    // TODO(thakis): Use a custom button cell.
    NSRect frame = NSMakeRect(
        kMarginWidth, y, NSWidth(documentFrame) - kMarginWidth, kEntryHeight);
    scoped_nsobject<NSButton> button([[NSButton alloc] initWithFrame:frame]);
    [button setButtonType:NSSwitchButton];
    [button setTitle:base::SysUTF16ToNSString(notifier->name)];

    [button setState:notifier->enabled ? NSOnState : NSOffState];
    [button setTag:i];
    [button setTarget:self];
    [button setAction:@selector(checkboxClicked:)];

    [documentView addSubview:button.release()];

    y += NSHeight(frame);
  }
  documentFrame.size.height = y;
  [documentView setFrame:documentFrame];

  // Scroll view for the notifier settings.
  NSRect scrollFrame = documentFrame;
  scrollFrame.origin.y = kMarginWidth;
  CGFloat remainingHeight =
      NSMinY(subheaderFrame) - message_center::kTextTopPadding -
      NSMinY(scrollFrame);

  if (NSHeight(documentFrame) < remainingHeight) {
    // Everything fits without scrolling.
    CGFloat delta = remainingHeight - NSHeight(documentFrame);
    headerFrame.origin.y -= delta;
    backButtonFrame.origin.y -= delta;
    subheaderFrame.origin.y -= delta;
    fullFrame.size.height -= delta;
  } else {
    scrollFrame.size.height = remainingHeight;
  }

  scrollView_.reset([[NSScrollView alloc] initWithFrame:scrollFrame]);
  [scrollView_ setAutohidesScrollers:YES];
  [scrollView_ setAutoresizingMask:NSViewMinYMargin];
  [scrollView_ setDocumentView:documentView];
  [scrollView_ setDrawsBackground:NO];
  [scrollView_ setHasHorizontalScroller:NO];
  [scrollView_ setHasVerticalScroller:YES];

  // Set final sizes.
  [[self view] setFrame:fullFrame];
  [[self view] addSubview:scrollView_];
  [backButton_ setFrame:backButtonFrame];
  [settingsText_ setFrame:headerFrame];
  [detailsText_ setFrame:subheaderFrame];
}

- (void)checkboxClicked:(id)sender {
  provider_->SetNotifierEnabled(*notifiers_[[sender tag]],
                                [sender state] == NSOnState);
}

- (message_center::NotifierSettingsDelegateMac*)delegate {
  return delegate_.get();
}

- (NSView*)responderView {
  return backButton_;
}

- (void)setCloseTarget:(id)target {
  [backButton_ setTarget:target];
}

- (void)setCloseAction:(SEL)action {
  [backButton_ setAction:action];
}

// Testing API /////////////////////////////////////////////////////////////////

- (NSScrollView*)scrollView {
  return scrollView_;
}

@end
