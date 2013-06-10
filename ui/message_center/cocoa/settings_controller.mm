// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/message_center/cocoa/settings_controller.h"

#include "base/mac/foundation_util.h"
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
const int kIconSize = 16;
const int kIconTextPadding = 2;

@interface MCSettingsButtonCell : NSButtonCell {
  // A checkbox's regular image is the checkmark image. This additional image
  // is used for the favicon or app icon shown next to the checkmark.
  scoped_nsobject<NSImage> extraImage_;
}
- (void)setExtraImage:(NSImage*)extraImage;
@end

@implementation MCSettingsButtonCell
- (void)setExtraImage:(NSImage*)extraImage {
  extraImage_.reset([extraImage retain]);
}

- (NSRect)drawTitle:(NSAttributedString*)title
          withFrame:(NSRect)frame
             inView:(NSView*)controlView {
  // drawTitle:withFrame:inView: draws the checkmark image. Draw the extra
  // image as part of the checkbox's text.
  if (extraImage_) {
    NSRect imageRect = frame;
    imageRect.size = NSMakeSize(kIconSize, kIconSize);
    [extraImage_ drawInRect:imageRect
                   fromRect:NSZeroRect
                  operation:NSCompositeSourceOver
                   fraction:1.0
             respectFlipped:YES
                      hints:nil];

    CGFloat inset = kIconSize + kIconTextPadding;
    frame.origin.x += inset;
    frame.size.width -= inset;
  }
  return [super drawTitle:title withFrame:frame inView:controlView];
}

- (NSUInteger)hitTestForEvent:(NSEvent*)event
                       inRect:(NSRect)cellFrame
                       ofView:(NSView*)controlView {
  NSUInteger result =
      [super hitTestForEvent:event inRect:cellFrame ofView:controlView];
  if (result == NSCellHitNone && extraImage_) {
    // The default button cell does hit testing on the attributed string. Since
    // this cell draws an icon in front of the string, tweak the hit testing
    // result.
    NSPoint point =
        [controlView convertPoint:[event locationInWindow] fromView:nil];

    NSRect rect = [self titleRectForBounds:[controlView bounds]];
    rect.size = [[self attributedTitle] size];
    rect.size.width += kIconSize + kIconTextPadding;

    if (NSPointInRect(point, rect))
      result = NSCellHitContentArea | NSCellHitTrackableArea;
  }
  return result;
}
@end

@interface MCSettingsController (Private)
// Sets the icon on the checkbox corresponding to |notifiers_[index]|.
- (void)setIcon:(NSImage*)icon forNotifierIndex:(size_t)index;

- (void)setIcon:(NSImage*)icon forAppId:(const std::string&)id;
- (void)setIcon:(NSImage*)icon forURL:(const GURL&)url;

// Returns the NSButton corresponding to the checkbox for |notifiers_[index]|.
- (NSButton*)buttonForNotifierAtIndex:(size_t)index;
@end

namespace message_center {

NotifierSettingsDelegateMac::~NotifierSettingsDelegateMac() {}

void NotifierSettingsDelegateMac::UpdateIconImage(const std::string& id,
                                                  const gfx::Image& icon) {
  [cocoa_controller() setIcon:icon.AsNSImage() forAppId:id];
}

void NotifierSettingsDelegateMac::UpdateFavicon(const GURL& url,
                                                const gfx::Image& icon) {
  [cocoa_controller() setIcon:icon.AsNSImage() forURL:url];
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
    scoped_nsobject<MCSettingsButtonCell> cell(
        [[MCSettingsButtonCell alloc]
            initTextCell:base::SysUTF16ToNSString(notifier->name)]);
    [button setCell:cell];
    [button setButtonType:NSSwitchButton];

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

// Private API /////////////////////////////////////////////////////////////////

- (void)setIcon:(NSImage*)icon forNotifierIndex:(size_t)index {
  NSButton* button = [self buttonForNotifierAtIndex:index];
  [[button cell] setExtraImage:icon];
  [button setNeedsDisplay:YES];
}

- (void)setIcon:(NSImage*)icon forAppId:(const std::string&)id {
  for (size_t i = 0; i < notifiers_.size(); ++i) {
    if (notifiers_[i]->id == id) {
      [self setIcon:icon forNotifierIndex:i];
      return;
    }
  }
}

- (void)setIcon:(NSImage*)icon forURL:(const GURL&)url {
  for (size_t i = 0; i < notifiers_.size(); ++i) {
    if (notifiers_[i]->url == url) {
      [self setIcon:icon forNotifierIndex:i];
      return;
    }
  }
}

- (NSButton*)buttonForNotifierAtIndex:(size_t)index {
  NSArray* subviews = [[scrollView_ documentView] subviews];
  // The checkboxes are in bottom-top order, the checkbox for notifiers_[0] is
  // last.
  NSView* view = [subviews objectAtIndex:notifiers_.size() - 1 - index];
  return base::mac::ObjCCastStrict<NSButton>(view);
}

@end
