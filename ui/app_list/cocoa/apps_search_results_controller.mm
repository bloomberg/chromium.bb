// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/app_list/cocoa/apps_search_results_controller.h"

#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/strings/sys_string_conversions.h"
#include "skia/ext/skia_utils_mac.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_model.h"
#import "ui/app_list/cocoa/apps_search_results_model_bridge.h"
#include "ui/app_list/search_result.h"
#import "ui/base/cocoa/flipped_view.h"
#include "ui/gfx/image/image_skia_util_mac.h"

namespace {

const CGFloat kPreferredRowHeight = 52;
const CGFloat kIconDimension = 32;
const CGFloat kIconPadding = 14;
const CGFloat kIconViewWidth = kIconDimension + 2 * kIconPadding;
const CGFloat kTextTrailPadding = kIconPadding;

// Map background styles to represent selection and hover in the results list.
const NSBackgroundStyle kBackgroundNormal = NSBackgroundStyleLight;
const NSBackgroundStyle kBackgroundSelected = NSBackgroundStyleDark;
const NSBackgroundStyle kBackgroundHovered = NSBackgroundStyleRaised;

}  // namespace

@interface AppsSearchResultsController ()

- (void)loadAndSetViewWithResultsFrameSize:(NSSize)size;
- (void)mouseDown:(NSEvent*)theEvent;
- (void)tableViewClicked:(id)sender;
- (app_list::AppListModel::SearchResults*)searchResults;
- (void)activateSelection;
- (BOOL)moveSelectionByDelta:(NSInteger)delta;
- (NSMenu*)contextMenuForRow:(NSInteger)rowIndex;

@end

@interface AppsSearchResultsCell : NSTextFieldCell
@end

// Immutable class representing a search result in the NSTableView.
@interface AppsSearchResultRep : NSObject<NSCopying> {
 @private
  base::scoped_nsobject<NSAttributedString> attributedStringValue_;
  base::scoped_nsobject<NSImage> resultIcon_;
}

@property(readonly, nonatomic) NSAttributedString* attributedStringValue;
@property(readonly, nonatomic) NSImage* resultIcon;

- (id)initWithSearchResult:(app_list::SearchResult*)result;

- (NSMutableAttributedString*)createRenderText:(const base::string16&)content
    tags:(const app_list::SearchResult::Tags&)tags;

- (NSAttributedString*)createResultsAttributedStringWithModel
    :(app_list::SearchResult*)result;

@end

// Simple extension to NSTableView that passes mouseDown events to the
// delegate so that drag events can be detected, and forwards requests for
// context menus.
@interface AppsSearchResultsTableView : NSTableView

- (AppsSearchResultsController*)controller;

@end

@implementation AppsSearchResultsController

@synthesize delegate = delegate_;

- (id)initWithAppsSearchResultsFrameSize:(NSSize)size {
  if ((self = [super init])) {
    hoveredRowIndex_ = -1;
    [self loadAndSetViewWithResultsFrameSize:size];
  }
  return self;
}

- (app_list::AppListModel::SearchResults*)results {
  DCHECK([delegate_ appListModel]);
  return [delegate_ appListModel]->results();
}

- (NSTableView*)tableView {
  return tableView_;
}

- (void)setDelegate:(id<AppsSearchResultsDelegate>)newDelegate {
  bridge_.reset();
  delegate_ = newDelegate;
  app_list::AppListModel* appListModel = [delegate_ appListModel];
  if (!appListModel || !appListModel->results()) {
    [tableView_ reloadData];
    return;
  }

  bridge_.reset(new app_list::AppsSearchResultsModelBridge(self));
  [tableView_ reloadData];
}

- (BOOL)handleCommandBySelector:(SEL)command {
  if (command == @selector(insertNewline:) ||
      command == @selector(insertLineBreak:)) {
    [self activateSelection];
    return YES;
  }

  if (command == @selector(moveUp:))
    return [self moveSelectionByDelta:-1];

  if (command == @selector(moveDown:))
    return [self moveSelectionByDelta:1];

  return NO;
}

- (void)loadAndSetViewWithResultsFrameSize:(NSSize)size {
  tableView_.reset(
      [[AppsSearchResultsTableView alloc] initWithFrame:NSZeroRect]);
  // Refuse first responder so that focus stays with the search text field.
  [tableView_ setRefusesFirstResponder:YES];
  [tableView_ setRowHeight:kPreferredRowHeight];
  [tableView_ setGridStyleMask:NSTableViewSolidHorizontalGridLineMask];
  [tableView_ setGridColor:
      gfx::SkColorToSRGBNSColor(app_list::kResultBorderColor)];
  [tableView_ setBackgroundColor:[NSColor clearColor]];
  [tableView_ setAction:@selector(tableViewClicked:)];
  [tableView_ setDelegate:self];
  [tableView_ setDataSource:self];
  [tableView_ setTarget:self];

  // Tracking to highlight an individual row on mouseover.
  trackingArea_.reset(
    [[CrTrackingArea alloc] initWithRect:NSZeroRect
                                 options:NSTrackingInVisibleRect |
                                         NSTrackingMouseEnteredAndExited |
                                         NSTrackingMouseMoved |
                                         NSTrackingActiveInKeyWindow
                                   owner:self
                                userInfo:nil]);
  [tableView_ addTrackingArea:trackingArea_.get()];

  base::scoped_nsobject<NSTableColumn> resultsColumn(
      [[NSTableColumn alloc] initWithIdentifier:@""]);
  base::scoped_nsobject<NSCell> resultsDataCell(
      [[AppsSearchResultsCell alloc] initTextCell:@""]);
  [resultsColumn setDataCell:resultsDataCell];
  [resultsColumn setWidth:size.width];
  [tableView_ addTableColumn:resultsColumn];

  // An NSTableView is normally put in a NSScrollView, but scrolling is not
  // used for the app list. Instead, place it in a container with the desired
  // size; flipped so the table is anchored to the top-left.
  base::scoped_nsobject<FlippedView> containerView([[FlippedView alloc]
      initWithFrame:NSMakeRect(0, 0, size.width, size.height)]);

  // The container is then anchored in an un-flipped view, initially hidden,
  // so that |containerView| slides in from the top when showing results.
  base::scoped_nsobject<NSView> clipView(
      [[NSView alloc] initWithFrame:NSMakeRect(0, 0, size.width, 0)]);

  [containerView addSubview:tableView_];
  [clipView addSubview:containerView];
  [self setView:clipView];
}

- (void)mouseDown:(NSEvent*)theEvent {
  lastMouseDownInView_ = [tableView_ convertPoint:[theEvent locationInWindow]
                                         fromView:nil];
}

- (void)tableViewClicked:(id)sender {
  const CGFloat kDragThreshold = 5;
  // If the user clicked and then dragged elsewhere, ignore the click.
  NSEvent* event = [[tableView_ window] currentEvent];
  NSPoint pointInView = [tableView_ convertPoint:[event locationInWindow]
                                        fromView:nil];
  CGFloat deltaX = pointInView.x - lastMouseDownInView_.x;
  CGFloat deltaY = pointInView.y - lastMouseDownInView_.y;
  if (deltaX * deltaX + deltaY * deltaY <= kDragThreshold * kDragThreshold)
    [self activateSelection];

  // Mouse tracking is suppressed by the NSTableView during a drag, so ensure
  // any hover state is cleaned up.
  [self mouseMoved:event];
}

- (app_list::AppListModel::SearchResults*)searchResults {
  app_list::AppListModel* appListModel = [delegate_ appListModel];
  DCHECK(bridge_);
  DCHECK(appListModel);
  DCHECK(appListModel->results());
  return appListModel->results();
}

- (void)activateSelection {
  NSInteger selectedRow = [tableView_ selectedRow];
  if (!bridge_ || selectedRow < 0)
    return;

  [delegate_ openResult:[self searchResults]->GetItemAt(selectedRow)];
}

- (BOOL)moveSelectionByDelta:(NSInteger)delta {
  NSInteger rowCount = [tableView_ numberOfRows];
  if (rowCount <= 0)
    return NO;

  NSInteger selectedRow = [tableView_ selectedRow];
  NSInteger targetRow;
  if (selectedRow == -1) {
    // No selection. Select first or last, based on direction.
    targetRow = delta > 0 ? 0 : rowCount - 1;
  } else {
    targetRow = (selectedRow + delta) % rowCount;
    if (targetRow < 0)
      targetRow += rowCount;
  }

  [tableView_ selectRowIndexes:[NSIndexSet indexSetWithIndex:targetRow]
          byExtendingSelection:NO];
  return YES;
}

- (NSMenu*)contextMenuForRow:(NSInteger)rowIndex {
  DCHECK(bridge_);
  if (rowIndex < 0)
    return nil;

  [tableView_ selectRowIndexes:[NSIndexSet indexSetWithIndex:rowIndex]
          byExtendingSelection:NO];
  return bridge_->MenuForItem(rowIndex);
}

- (NSInteger)numberOfRowsInTableView:(NSTableView*)aTableView {
  return bridge_ ? [self searchResults]->item_count() : 0;
}

- (id)tableView:(NSTableView*)aTableView
    objectValueForTableColumn:(NSTableColumn*)aTableColumn
                          row:(NSInteger)rowIndex {
  // When the results were previously cleared, nothing will be selected. For
  // that case, select the first row when it appears.
  if (rowIndex == 0 && [tableView_ selectedRow] == -1) {
    [tableView_ selectRowIndexes:[NSIndexSet indexSetWithIndex:0]
            byExtendingSelection:NO];
  }

  base::scoped_nsobject<AppsSearchResultRep> resultRep(
      [[AppsSearchResultRep alloc]
          initWithSearchResult:[self searchResults]->GetItemAt(rowIndex)]);
  return resultRep.autorelease();
}

- (void)tableView:(NSTableView*)tableView
    willDisplayCell:(id)cell
     forTableColumn:(NSTableColumn*)tableColumn
                row:(NSInteger)rowIndex {
  if (rowIndex == [tableView selectedRow])
    [cell setBackgroundStyle:kBackgroundSelected];
  else if (rowIndex == hoveredRowIndex_)
    [cell setBackgroundStyle:kBackgroundHovered];
  else
    [cell setBackgroundStyle:kBackgroundNormal];
}

- (void)mouseExited:(NSEvent*)theEvent {
  if (hoveredRowIndex_ == -1)
    return;

  [tableView_ setNeedsDisplayInRect:[tableView_ rectOfRow:hoveredRowIndex_]];
  hoveredRowIndex_ = -1;
}

- (void)mouseMoved:(NSEvent*)theEvent {
  NSPoint pointInView = [tableView_ convertPoint:[theEvent locationInWindow]
                                        fromView:nil];
  NSInteger newIndex = [tableView_ rowAtPoint:pointInView];
  if (newIndex == hoveredRowIndex_)
    return;

  if (newIndex != -1)
    [tableView_ setNeedsDisplayInRect:[tableView_ rectOfRow:newIndex]];
  if (hoveredRowIndex_ != -1)
    [tableView_ setNeedsDisplayInRect:[tableView_ rectOfRow:hoveredRowIndex_]];
  hoveredRowIndex_ = newIndex;
}

@end

@implementation AppsSearchResultRep

- (NSAttributedString*)attributedStringValue {
  return attributedStringValue_;
}

- (NSImage*)resultIcon {
  return resultIcon_;
}

- (id)initWithSearchResult:(app_list::SearchResult*)result {
  if ((self = [super init])) {
    attributedStringValue_.reset(
        [[self createResultsAttributedStringWithModel:result] retain]);
    if (!result->icon().isNull()) {
      resultIcon_.reset([gfx::NSImageFromImageSkiaWithColorSpace(
          result->icon(), base::mac::GetSRGBColorSpace()) retain]);
    }
  }
  return self;
}

- (NSMutableAttributedString*)createRenderText:(const base::string16&)content
    tags:(const app_list::SearchResult::Tags&)tags {
  NSFont* boldFont = nil;
  base::scoped_nsobject<NSMutableParagraphStyle> paragraphStyle(
      [[NSMutableParagraphStyle alloc] init]);
  [paragraphStyle setLineBreakMode:NSLineBreakByTruncatingTail];
  NSDictionary* defaultAttributes = @{
      NSForegroundColorAttributeName:
          gfx::SkColorToSRGBNSColor(app_list::kResultDefaultTextColor),
      NSParagraphStyleAttributeName: paragraphStyle
  };

  base::scoped_nsobject<NSMutableAttributedString> text(
      [[NSMutableAttributedString alloc]
          initWithString:base::SysUTF16ToNSString(content)
              attributes:defaultAttributes]);

  for (app_list::SearchResult::Tags::const_iterator it = tags.begin();
       it != tags.end(); ++it) {
    if (it->styles == app_list::SearchResult::Tag::NONE)
      continue;

    if (it->styles & app_list::SearchResult::Tag::MATCH) {
      if (!boldFont) {
        NSFontManager* fontManager = [NSFontManager sharedFontManager];
        boldFont = [fontManager convertFont:[NSFont controlContentFontOfSize:0]
                                toHaveTrait:NSBoldFontMask];
      }
      [text addAttribute:NSFontAttributeName
                   value:boldFont
                   range:it->range.ToNSRange()];
    }

    if (it->styles & app_list::SearchResult::Tag::DIM) {
      NSColor* dimmedColor =
          gfx::SkColorToSRGBNSColor(app_list::kResultDimmedTextColor);
      [text addAttribute:NSForegroundColorAttributeName
                   value:dimmedColor
                   range:it->range.ToNSRange()];
    } else if (it->styles & app_list::SearchResult::Tag::URL) {
      NSColor* urlColor =
          gfx::SkColorToSRGBNSColor(app_list::kResultURLTextColor);
      [text addAttribute:NSForegroundColorAttributeName
                   value:urlColor
                   range:it->range.ToNSRange()];
    }
  }

  return text.autorelease();
}

- (NSAttributedString*)createResultsAttributedStringWithModel
    :(app_list::SearchResult*)result {
  NSMutableAttributedString* titleText =
      [self createRenderText:result->title()
                        tags:result->title_tags()];
  if (!result->details().empty()) {
    NSMutableAttributedString* detailText =
        [self createRenderText:result->details()
                          tags:result->details_tags()];
    base::scoped_nsobject<NSAttributedString> lineBreak(
        [[NSAttributedString alloc] initWithString:@"\n"]);
    [titleText appendAttributedString:lineBreak];
    [titleText appendAttributedString:detailText];
  }
  return titleText;
}

- (id)copyWithZone:(NSZone*)zone {
  return [self retain];
}

@end

@implementation AppsSearchResultsTableView

- (AppsSearchResultsController*)controller {
  return base::mac::ObjCCastStrict<AppsSearchResultsController>(
      [self delegate]);
}

- (void)mouseDown:(NSEvent*)theEvent {
  [[self controller] mouseDown:theEvent];
  [super mouseDown:theEvent];
}

- (NSMenu*)menuForEvent:(NSEvent*)theEvent {
  NSPoint pointInView = [self convertPoint:[theEvent locationInWindow]
                                  fromView:nil];
  return [[self controller] contextMenuForRow:[self rowAtPoint:pointInView]];
}

@end

@implementation AppsSearchResultsCell

- (void)drawWithFrame:(NSRect)cellFrame
               inView:(NSView*)controlView {
  if ([self backgroundStyle] != kBackgroundNormal) {
    if ([self backgroundStyle] == kBackgroundSelected)
      [gfx::SkColorToSRGBNSColor(app_list::kSelectedColor) set];
    else
      [gfx::SkColorToSRGBNSColor(app_list::kHighlightedColor) set];

    // Extend up by one pixel to draw over cell border.
    NSRect backgroundRect = cellFrame;
    backgroundRect.origin.y -= 1;
    backgroundRect.size.height += 1;
    NSRectFill(backgroundRect);
  }

  NSAttributedString* titleText = [self attributedStringValue];
  NSRect titleRect = cellFrame;
  titleRect.size.width -= kTextTrailPadding + kIconViewWidth;
  titleRect.origin.x += kIconViewWidth;
  titleRect.origin.y +=
      floor(NSHeight(cellFrame) / 2 - [titleText size].height / 2);
  // Ensure no drawing occurs outside of the cell.
  titleRect = NSIntersectionRect(titleRect, cellFrame);

  [titleText drawInRect:titleRect];

  NSImage* resultIcon = [[self objectValue] resultIcon];
  if (!resultIcon)
    return;

  NSSize iconSize = [resultIcon size];
  NSRect iconRect = NSMakeRect(
      floor(NSMinX(cellFrame) + kIconViewWidth / 2 - iconSize.width / 2),
      floor(NSMinY(cellFrame) + kPreferredRowHeight / 2 - iconSize.height / 2),
      std::min(iconSize.width, kIconDimension),
      std::min(iconSize.height, kIconDimension));
  [resultIcon drawInRect:iconRect
                fromRect:NSZeroRect
               operation:NSCompositeSourceOver
                fraction:1.0
          respectFlipped:YES
                   hints:nil];
}

@end
