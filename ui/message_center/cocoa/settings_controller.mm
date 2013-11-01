// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/message_center/cocoa/settings_controller.h"

#include "base/mac/foundation_util.h"
#import "base/mac/scoped_nsobject.h"
#include "base/stl_util.h"
#include "base/strings/sys_string_conversions.h"
#include "grit/ui_strings.h"
#include "skia/ext/skia_utils_mac.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#import "ui/message_center/cocoa/tray_view_controller.h"
#include "ui/message_center/message_center_style.h"

const int kMarginWidth = 16;
const int kEntryHeight = 38;
const int kIconSize = 16;
const int kIconTextPadding = 8;
const int kCheckmarkIconPadding = 16;

const int kIntrinsicCheckmarkPadding = 4;  // Padding already provided by Cocoa.
const int kCorrectedCheckmarkPadding =
    kCheckmarkIconPadding - kIntrinsicCheckmarkPadding;

@interface MCSettingsButtonCell : NSButtonCell {
  // A checkbox's regular image is the checkmark image. This additional image
  // is used for the favicon or app icon shown next to the checkmark.
  base::scoped_nsobject<NSImage> extraImage_;
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
  CGFloat inset = kCorrectedCheckmarkPadding;
  // drawTitle:withFrame:inView: draws the checkmark image. Draw the extra
  // image as part of the checkbox's text.
  if (extraImage_) {
    NSRect imageRect = frame;
    imageRect.origin.x += inset;
    imageRect.size = NSMakeSize(kIconSize, kIconSize);
    [extraImage_ drawInRect:imageRect
                   fromRect:NSZeroRect
                  operation:NSCompositeSourceOver
                   fraction:1.0
             respectFlipped:YES
                      hints:nil];

    inset += kIconSize + kIconTextPadding;
  }
  frame.origin.x += inset;
  frame.size.width -= inset;
  return [super drawTitle:title withFrame:frame inView:controlView];
}

- (NSUInteger)hitTestForEvent:(NSEvent*)event
                       inRect:(NSRect)cellFrame
                       ofView:(NSView*)controlView {
  NSUInteger result =
      [super hitTestForEvent:event inRect:cellFrame ofView:controlView];
  if (result == NSCellHitNone) {
    // The default button cell does hit testing on the attributed string. Since
    // this cell draws additional spacing and an icon in front of the string,
    // tweak the hit testing result.
    NSPoint point =
        [controlView convertPoint:[event locationInWindow] fromView:nil];

    NSRect rect = [self titleRectForBounds:[controlView bounds]];
    rect.size = [[self attributedTitle] size];
    rect.size.width += kCorrectedCheckmarkPadding;

    if (extraImage_) {
      rect.size.width +=
          kIconSize + kIconTextPadding + kCorrectedCheckmarkPadding;
    }

    if (NSPointInRect(point, rect))
      result = NSCellHitContentArea | NSCellHitTrackableArea;
  }
  return result;
}
@end

@interface MCSettingsController (Private)
// Sets the icon on the checkbox corresponding to |notifiers_[index]|.
- (void)setIcon:(NSImage*)icon forNotifierIndex:(size_t)index;

- (void)setIcon:(NSImage*)icon
  forNotifierId:(const message_center::NotifierId&)id;

// Returns the NSButton corresponding to the checkbox for |notifiers_[index]|.
- (NSButton*)buttonForNotifierAtIndex:(size_t)index;

// Update the contents view.
- (void)updateView;

// Handler for the notifier group dropdown menu.
- (void)notifierGroupSelectionChanged:(id)sender;
@end

namespace message_center {

NotifierSettingsObserverMac::~NotifierSettingsObserverMac() {}

void NotifierSettingsObserverMac::UpdateIconImage(const NotifierId& notifier_id,
                                                  const gfx::Image& icon) {
  [settings_controller_ setIcon:icon.AsNSImage() forNotifierId:notifier_id];
}

void NotifierSettingsObserverMac::NotifierGroupChanged() {
  [settings_controller_ updateView];
}

}  // namespace message_center

@implementation MCSettingsController

- (id)initWithProvider:(message_center::NotifierSettingsProvider*)provider
    trayViewController:(MCTrayViewController*)trayViewController {
  if ((self = [super initWithNibName:nil bundle:nil])) {
    observer_.reset(new message_center::NotifierSettingsObserverMac(self));
    provider_ = provider;
    trayViewController_ = trayViewController;
    provider_->AddObserver(observer_.get());
  }
  return self;
}

- (void)dealloc {
  provider_->RemoveObserver(observer_.get());
  provider_->OnNotifierSettingsClosing();
  STLDeleteElements(&notifiers_);
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

- (void)updateView {
  notifiers_.clear();
  [trayViewController_ updateSettings];
}

- (void)loadView {
  DCHECK(notifiers_.empty());
  provider_->GetNotifierList(&notifiers_);
  CGFloat maxHeight = [MCTrayViewController maxTrayClientHeight];

  // Container view.
  NSRect fullFrame =
      NSMakeRect(0, 0, [MCTrayViewController trayWidth], maxHeight);
  base::scoped_nsobject<NSBox> view([[NSBox alloc] initWithFrame:fullFrame]);
  [view setBorderType:NSNoBorder];
  [view setBoxType:NSBoxCustom];
  [view setContentViewMargins:NSZeroSize];
  [view setFillColor:gfx::SkColorToCalibratedNSColor(
      message_center::kMessageCenterBackgroundColor)];
  [view setTitlePosition:NSNoTitle];
  [self setView:view];

  // "Settings" text.
  NSRect headerFrame = NSMakeRect(
      kMarginWidth, kMarginWidth, NSWidth(fullFrame), NSHeight(fullFrame));
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

  // Subheader.
  NSRect subheaderFrame = NSMakeRect(
      kMarginWidth, kMarginWidth, NSWidth(fullFrame), NSHeight(fullFrame));
  detailsText_.reset([self newLabelWithFrame:subheaderFrame]);
  [detailsText_ setAutoresizingMask:NSViewMinYMargin];
  [detailsText_ setTextColor:gfx::SkColorToCalibratedNSColor(
      message_center::kDimTextColor)];
  [detailsText_ setFont:
      [NSFont messageFontOfSize:message_center::kMessageFontSize]];

  size_t groupCount = provider_->GetNotifierGroupCount();
  [detailsText_ setStringValue:l10n_util::GetNSString(
      groupCount > 1 ? IDS_MESSAGE_CENTER_SETTINGS_DESCRIPTION_MULTIUSER
                     : IDS_MESSAGE_CENTER_SETTINGS_DIALOG_DESCRIPTION)];
  [detailsText_ sizeToFit];
  subheaderFrame = [detailsText_ frame];
  subheaderFrame.origin.y =
      NSMinY(headerFrame) - message_center::kTextTopPadding -
      NSHeight(subheaderFrame);
  [[self view] addSubview:detailsText_];

  // Profile switcher is only needed for more than one profile.
  NSRect dropDownButtonFrame = subheaderFrame;
  if (groupCount > 1) {
    dropDownButtonFrame = NSMakeRect(
        kMarginWidth, kMarginWidth, NSWidth(fullFrame), NSHeight(fullFrame));
    groupDropDownButton_.reset(
        [[NSPopUpButton alloc] initWithFrame:dropDownButtonFrame
                                   pullsDown:YES]);
    [groupDropDownButton_ setAction:@selector(notifierGroupSelectionChanged:)];
    [groupDropDownButton_ setTarget:self];
    // Add a dummy item for pull-down.
    [groupDropDownButton_ addItemWithTitle:@""];
    string16 title;
    for (size_t i = 0; i < groupCount; ++i) {
      const message_center::NotifierGroup& group =
          provider_->GetNotifierGroupAt(i);
      string16 item = group.login_info.empty() ? group.name : group.login_info;
      [groupDropDownButton_ addItemWithTitle:base::SysUTF16ToNSString(item)];
      if (provider_->IsNotifierGroupActiveAt(i)) {
        title = item;
        [[groupDropDownButton_ lastItem] setState:NSOnState];
      }
    }
    [groupDropDownButton_ setTitle:base::SysUTF16ToNSString(title)];
    [groupDropDownButton_ sizeToFit];
    dropDownButtonFrame = [groupDropDownButton_ frame];
    dropDownButtonFrame.origin.y =
        NSMinY(subheaderFrame) - message_center::kTextTopPadding -
        NSHeight(dropDownButtonFrame);
    dropDownButtonFrame.size.width = NSWidth(fullFrame) - 2 * kMarginWidth;
    [[self view] addSubview:groupDropDownButton_];
  }

  // Document view for the notifier settings.
  CGFloat y = 0;
  NSRect documentFrame = NSMakeRect(0, 0, NSWidth(fullFrame), 0);
  base::scoped_nsobject<NSView> documentView(
      [[NSView alloc] initWithFrame:documentFrame]);
  for (int i = notifiers_.size() - 1; i >= 0; --i) {
    message_center::Notifier* notifier = notifiers_[i];

    // TODO(thakis): Use a custom button cell.
    NSRect frame = NSMakeRect(
        kMarginWidth, y, NSWidth(documentFrame) - kMarginWidth, kEntryHeight);
    base::scoped_nsobject<NSButton> button(
        [[NSButton alloc] initWithFrame:frame]);
    base::scoped_nsobject<MCSettingsButtonCell> cell(
        [[MCSettingsButtonCell alloc]
            initTextCell:base::SysUTF16ToNSString(notifier->name)]);
    if (!notifier->icon.IsEmpty())
      [cell setExtraImage:notifier->icon.AsNSImage()];
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
      NSMinY(dropDownButtonFrame) - message_center::kTextTopPadding -
      NSMinY(scrollFrame);

  if (NSHeight(documentFrame) < remainingHeight) {
    // Everything fits without scrolling.
    CGFloat delta = remainingHeight - NSHeight(documentFrame);
    headerFrame.origin.y -= delta;
    subheaderFrame.origin.y -= delta;
    dropDownButtonFrame.origin.y -= delta;
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

  // Scroll to top.
  NSPoint newScrollOrigin =
      NSMakePoint(0.0,
                  NSMaxY([[scrollView_ documentView] frame]) -
                      NSHeight([[scrollView_ contentView] bounds]));
  [[scrollView_ documentView] scrollPoint:newScrollOrigin];

  // Set final sizes.
  [[self view] setFrame:fullFrame];
  [[self view] addSubview:scrollView_];
  [settingsText_ setFrame:headerFrame];
  [detailsText_ setFrame:subheaderFrame];
  [groupDropDownButton_ setFrame:dropDownButtonFrame];
}

- (void)checkboxClicked:(id)sender {
  provider_->SetNotifierEnabled(*notifiers_[[sender tag]],
                                [sender state] == NSOnState);
}

// Testing API /////////////////////////////////////////////////////////////////

- (NSPopUpButton*)groupDropDownButton {
  return groupDropDownButton_;
}

- (NSScrollView*)scrollView {
  return scrollView_;
}

// Private API /////////////////////////////////////////////////////////////////

- (void)setIcon:(NSImage*)icon forNotifierIndex:(size_t)index {
  NSButton* button = [self buttonForNotifierAtIndex:index];
  [[button cell] setExtraImage:icon];
  [button setNeedsDisplay:YES];
}

- (void)setIcon:(NSImage*)icon
  forNotifierId:(const message_center::NotifierId&)id {
  for (size_t i = 0; i < notifiers_.size(); ++i) {
    if (notifiers_[i]->notifier_id == id) {
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

- (void)notifierGroupSelectionChanged:(id)sender {
  DCHECK_EQ(groupDropDownButton_.get(), sender);
  NSPopUpButton* button = static_cast<NSPopUpButton*>(sender);
  // The first item is a dummy item.
  provider_->SwitchToNotifierGroup([button indexOfSelectedItem] - 1);
}

@end
