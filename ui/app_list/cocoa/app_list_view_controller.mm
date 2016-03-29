// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/app_list/cocoa/app_list_view_controller.h"

#include <stddef.h>

#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "skia/ext/skia_utils_mac.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/app_list_view_delegate.h"
#include "ui/app_list/app_list_view_delegate_observer.h"
#import "ui/app_list/cocoa/app_list_pager_view.h"
#import "ui/app_list/cocoa/apps_grid_controller.h"
#include "ui/app_list/search_box_model.h"
#import "ui/base/cocoa/flipped_view.h"
#import "ui/gfx/image/image_skia_util_mac.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"

namespace {

// The roundedness of the corners of the bubble.
const CGFloat kBubbleCornerRadius = 3;

// Height of the pager.
const CGFloat kPagerPreferredHeight = 57;

// Height of separator line drawn between the searchbox and grid view.
const CGFloat kTopSeparatorSize = 1;

// Height of the search input.
const CGFloat kSearchInputHeight = 48;

// Minimum margin on either side of the pager. If the pager grows beyond this,
// the segment size is reduced.
const CGFloat kMinPagerMargin = 40;
// Maximum width of a single segment.
const CGFloat kMaxSegmentWidth = 80;

// Duration of the animation for sliding in and out search results.
const NSTimeInterval kResultsAnimationDuration = 0.2;

// Properties of the message rectangle, if it is shown.
const NSRect kMessageRect = {{12, 12}, {370, 91}};
const CGFloat kMessageCornerRadius = 2;
const CGFloat kSpacingBelowMessageTitle = 6;
const SkColor kMessageBackgroundColor = SkColorSetRGB(0xFF, 0xFD, 0xE7);
const SkColor kMessageStrokeColor = SkColorSetARGB(0x3d, 0x00, 0x00, 0x00);
// The inset should be 16px, but NSTextView has its own inset of 3.
const CGFloat kMessageTextInset = 13;

}  // namespace

@interface BackgroundView : FlippedView;
@end

@implementation BackgroundView

- (void)drawRect:(NSRect)dirtyRect {
  gfx::ScopedNSGraphicsContextSaveGState context;
  NSRect boundsRect = [self bounds];
  NSRect searchAreaRect = NSMakeRect(0, 0,
                                     NSWidth(boundsRect), kSearchInputHeight);
  NSRect separatorRect = NSMakeRect(0, NSMaxY(searchAreaRect),
                                    NSWidth(boundsRect), kTopSeparatorSize);

  [[NSBezierPath bezierPathWithRoundedRect:boundsRect
                                   xRadius:kBubbleCornerRadius
                                   yRadius:kBubbleCornerRadius] addClip];

  [skia::SkColorToSRGBNSColor(app_list::kContentsBackgroundColor) set];
  NSRectFill(boundsRect);
  [skia::SkColorToSRGBNSColor(app_list::kSearchBoxBackground) set];
  NSRectFill(searchAreaRect);
  [skia::SkColorToSRGBNSColor(app_list::kTopSeparatorColor) set];
  NSRectFill(separatorRect);
}

@end

@interface MessageBackgroundView : FlippedView
@end

@implementation MessageBackgroundView

- (void)drawRect:(NSRect)dirtyRect {
  NSRect boundsRect = [self bounds];
  gfx::ScopedNSGraphicsContextSaveGState context;
  [[NSBezierPath bezierPathWithRoundedRect:boundsRect
                                   xRadius:kMessageCornerRadius
                                   yRadius:kMessageCornerRadius] addClip];

  [skia::SkColorToSRGBNSColor(kMessageStrokeColor) set];
  NSRectFill(boundsRect);

  [[NSBezierPath bezierPathWithRoundedRect:NSInsetRect(boundsRect, 1, 1)
                                   xRadius:kMessageCornerRadius
                                   yRadius:kMessageCornerRadius] addClip];
  [skia::SkColorToSRGBNSColor(kMessageBackgroundColor) set];
  NSRectFill(boundsRect);
}

@end

@interface AppListViewController ()

- (void)updateMessage;
- (void)loadAndSetView;
- (void)revealSearchResults:(BOOL)show;

@end

namespace app_list {

class AppListModelObserverBridge : public AppListViewDelegateObserver {
 public:
  AppListModelObserverBridge(AppListViewController* parent);
  ~AppListModelObserverBridge() override;

 private:
  // Overridden from app_list::AppListViewDelegateObserver:
  void OnProfilesChanged() override;
  void OnShutdown() override;

  AppListViewController* parent_;  // Weak. Owns us.

  DISALLOW_COPY_AND_ASSIGN(AppListModelObserverBridge);
};

AppListModelObserverBridge::AppListModelObserverBridge(
    AppListViewController* parent)
    : parent_(parent) {
  [parent_ delegate]->AddObserver(this);
}

AppListModelObserverBridge::~AppListModelObserverBridge() {
  [parent_ delegate]->RemoveObserver(this);
}

void AppListModelObserverBridge::OnProfilesChanged() {
  [parent_ onProfilesChanged];
}

void AppListModelObserverBridge::OnShutdown() {
  [parent_ setDelegate:nil];
}

}  // namespace app_list

@implementation AppListViewController

- (id)init {
  if ((self = [super init])) {
    appsGridController_.reset([[AppsGridController alloc] init]);
    [self loadAndSetView];

    [self totalPagesChanged];
    [self selectedPageChanged:0];
    [appsGridController_ setPaginationObserver:self];
  }
  return self;
}

- (void)dealloc {
  // Ensure that setDelegate(NULL) has been called before destruction, because
  // dealloc can be called at odd times, and Objective C destruction order does
  // not properly tear down these dependencies.
  DCHECK(delegate_ == NULL);
  [appsGridController_ setPaginationObserver:nil];
  [super dealloc];
}

- (AppsSearchBoxController*)searchBoxController {
  return appsSearchBoxController_;
}

- (BOOL)showingSearchResults {
  return showingSearchResults_;
}

- (AppsGridController*)appsGridController {
  return appsGridController_;
}

- (NSSegmentedControl*)pagerControl {
  return pagerControl_;
}

- (NSView*)backgroundView {
  return backgroundView_;
}

- (app_list::AppListViewDelegate*)delegate {
  return delegate_;
}

- (void)setDelegate:(app_list::AppListViewDelegate*)newDelegate {
  if (delegate_) {
    // Ensure the search box is cleared when switching profiles.
    if ([self searchBoxModel])
      [self searchBoxModel]->SetText(base::string16());

    // First clean up, in reverse order.
    app_list_model_observer_bridge_.reset();
    [appsSearchResultsController_ setDelegate:nil];
    [appsSearchBoxController_ setDelegate:nil];
    [appsGridController_ setDelegate:nil];
    [messageText_ setDelegate:nil];
  }
  delegate_ = newDelegate;
  if (delegate_) {
    [loadingIndicator_ stopAnimation:self];
  } else {
    [loadingIndicator_ startAnimation:self];
    return;
  }

  [appsGridController_ setDelegate:delegate_];
  [appsSearchBoxController_ setDelegate:self];
  [appsSearchResultsController_ setDelegate:self];
  app_list_model_observer_bridge_.reset(
      new app_list::AppListModelObserverBridge(self));
  [self onProfilesChanged];
  [self updateMessage];
}

- (void)updateMessage {
  if (![AppsGridController hasFewerRows])
    return;

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  NSFont* messageFont = rb.GetFontWithDelta(0).GetNativeFont();
  NSFont* titleFont = rb.GetFontWithDelta(2).GetNativeFont();

  base::string16 title = delegate_->GetMessageTitle();
  size_t messageBreak;
  base::string16 messageFull = delegate_->GetMessageText(&messageBreak);
  base::string16 shortcutName = delegate_->GetAppsShortcutName();
  base::string16 learnMore = delegate_->GetLearnMoreText();
  base::string16 learnMoreUrl = delegate_->GetLearnMoreLink();

  base::string16 messagePre = messageFull.substr(0, messageBreak);
  base::string16 messagePost = messageFull.substr(messageBreak);

  NSURL* linkURL = [NSURL URLWithString:base::SysUTF16ToNSString(learnMoreUrl)];
  gfx::ImageSkia* icon = delegate_->GetAppsIcon();

  // Shift the baseline up so that the graphics align centered. 4 looks nice. It
  // happens to be the image size minus the font size, but that's a coincidence.
  const CGFloat kBaselineShift = 4;
  base::scoped_nsobject<NSMutableParagraphStyle> paragraphStyle(
      [[NSMutableParagraphStyle alloc] init]);
  [paragraphStyle setLineSpacing:kSpacingBelowMessageTitle + kBaselineShift];

  NSNumber* baselineOffset = [NSNumber numberWithFloat:kBaselineShift];
  base::scoped_nsobject<NSMutableAttributedString> text(
      [[NSMutableAttributedString alloc]
          initWithString:base::SysUTF16ToNSString(title)
              attributes:@{
                NSParagraphStyleAttributeName : paragraphStyle,
                NSFontAttributeName : titleFont
              }]);

  NSDictionary* defaultAttributes = @{
    NSFontAttributeName : messageFont,
    NSBaselineOffsetAttributeName : baselineOffset
  };

  base::scoped_nsobject<NSAttributedString> lineBreak(
      [[NSAttributedString alloc] initWithString:@"\n"
                                      attributes:defaultAttributes]);
  base::scoped_nsobject<NSAttributedString> space([[NSAttributedString alloc]
      initWithString:@" "
          attributes:defaultAttributes]);
  base::scoped_nsobject<NSAttributedString> messagePreString(
      [[NSAttributedString alloc]
          initWithString:base::SysUTF16ToNSString(messagePre)
              attributes:defaultAttributes]);
  base::scoped_nsobject<NSAttributedString> messagePostString(
      [[NSAttributedString alloc]
          initWithString:base::SysUTF16ToNSString(messagePost)
              attributes:defaultAttributes]);

  // NSUnderlineStyleNone is broken.
  base::scoped_nsobject<NSAttributedString> learnMoreString(
      [[NSAttributedString alloc]
          initWithString:base::SysUTF16ToNSString(learnMore)
              attributes:@{
                NSParagraphStyleAttributeName : paragraphStyle,
                NSFontAttributeName : messageFont,
                NSLinkAttributeName : linkURL,
                NSBaselineOffsetAttributeName : baselineOffset,
                NSUnderlineStyleAttributeName :
                    [NSNumber numberWithInt:NSUnderlineStyleNone]
              }]);
  base::scoped_nsobject<NSAttributedString> shortcutStringText(
      [[NSAttributedString alloc]
          initWithString:base::SysUTF16ToNSString(shortcutName)
              attributes:defaultAttributes]);
  base::scoped_nsobject<NSMutableAttributedString> shortcutString(
      [[NSMutableAttributedString alloc] init]);
  if (icon) {
    NSImage* image = gfx::NSImageFromImageSkia(*icon);
    // The image has a bunch of representations. Ensure the smallest is used.
    // (Going smaller would make pixels all manky, so don't do that).
    [image setSize:NSMakeSize(16, 16)];

    base::scoped_nsobject<NSTextAttachmentCell> attachmentCell(
        [[NSTextAttachmentCell alloc] initImageCell:image]);
    base::scoped_nsobject<NSTextAttachment> attachment(
        [[NSTextAttachment alloc] init]);
    [attachment setAttachmentCell:attachmentCell];
    [shortcutString
        appendAttributedString:[NSAttributedString
                                   attributedStringWithAttachment:attachment]];
    [shortcutString appendAttributedString:space];
  }
  [shortcutString appendAttributedString:shortcutStringText];

  [text appendAttributedString:lineBreak];
  [text appendAttributedString:messagePreString];
  [text appendAttributedString:shortcutString];
  [text appendAttributedString:messagePostString];
  [text appendAttributedString:space];
  [text appendAttributedString:learnMoreString];

  [[messageText_ textStorage] setAttributedString:text];
  [messageText_ sizeToFit];

  // If the user scroller preference is to always show scrollbars, and the
  // translated message is long, the scroll track may be present. This means
  // text will be under the scroller. We only want vertical scrolling, but
  // reducing the width puts the scroll track in a weird spot. So, increase the
  // width of the scroll view to move the track into the padding towards the
  // message background border, then reduce the width of the text view. The
  // non-overlay scroller still looks kinda weird but hopefully not many will
  // actually see it.
  CGFloat overlap =
      NSWidth([messageText_ bounds]) - [messageScrollView_ contentSize].width;
  if (overlap > 0) {
    NSRect rect = [messageScrollView_ frame];
    rect.size.width += kMessageTextInset - 2;
    [messageScrollView_ setFrame:rect];
    overlap -= kMessageTextInset - 2;
    DCHECK_GT(overlap, 0);
    rect = [messageText_ frame];
    rect.size.width -= overlap;
    [messageText_ setFrame:rect];
    [messageText_ sizeToFit];

    // And after doing all that for some reason Cocoa scrolls to the bottom. So
    // fix that.
    [[messageScrollView_ documentView] scrollPoint:NSMakePoint(0, 0)];
  }

  [messageText_ setDelegate:self];
}

- (void)loadAndSetView {
  pagerControl_.reset([[AppListPagerView alloc] init]);
  [pagerControl_ setTarget:appsGridController_];
  [pagerControl_ setAction:@selector(onPagerClicked:)];

  NSRect gridFrame = [[appsGridController_ view] frame];

  base::scoped_nsobject<NSView> messageTextBackground;
  if ([AppsGridController hasFewerRows]) {
    messageTextBackground.reset(
        [[MessageBackgroundView alloc] initWithFrame:kMessageRect]);
    NSRect frameRect =
        NSInsetRect(kMessageRect, kMessageTextInset, kMessageTextInset);
    messageText_.reset([[NSTextView alloc] initWithFrame:frameRect]);
    // Provide a solid background here (as well as the background) so that
    // subpixel AA works.
    [messageText_
        setBackgroundColor:skia::SkColorToSRGBNSColor(kMessageBackgroundColor)];
    [messageText_ setDrawsBackground:YES];
    [messageText_ setEditable:NO];
    // Ideally setSelectable:NO would also be set here, but that disables mouse
    // events completely, breaking the "Learn more" link. Instead, selection is
    // "disabled" via a delegate method which Apple's documentation suggests. In
    // reality, selection still happens, it just disappears once the mouse is
    // released. To avoid the selection appearing, also set selected text to
    // have no special attributes. Sadly, the mouse cursor still displays an
    // I-beam, but hacking cursor rectangles on the view so that the "Learn
    // More" link is still correctly handled is too hard.
    [messageText_ setSelectedTextAttributes:@{}];
    gridFrame.origin.y += NSMaxY([messageTextBackground frame]);
  }

  [[appsGridController_ view] setFrame:gridFrame];

  NSRect contentsRect =
      NSMakeRect(0, kSearchInputHeight + kTopSeparatorSize, NSWidth(gridFrame),
                 NSMaxY(gridFrame) + kPagerPreferredHeight -
                     [AppsGridController scrollerPadding]);

  contentsView_.reset([[FlippedView alloc] initWithFrame:contentsRect]);

  // The contents view contains animations both from an NSCollectionView and the
  // app list's own transitive drag layers. On Mavericks, the subviews need to
  // have access to a compositing layer they can share. Otherwise the compositor
  // makes tearing artifacts. However, doing this on Mountain Lion or earler
  // results in flickering whilst an item is installing.
  if (base::mac::IsOSMavericksOrLater())
    [contentsView_ setWantsLayer:YES];

  backgroundView_.reset(
      [[BackgroundView alloc] initWithFrame:
              NSMakeRect(0, 0, NSMaxX(contentsRect), NSMaxY(contentsRect))]);
  appsSearchBoxController_.reset(
      [[AppsSearchBoxController alloc] initWithFrame:
          NSMakeRect(0, 0, NSWidth(contentsRect), kSearchInputHeight)]);
  appsSearchResultsController_.reset(
      [[AppsSearchResultsController alloc] initWithAppsSearchResultsFrameSize:
          [contentsView_ bounds].size]);
  base::scoped_nsobject<NSView> containerView(
      [[NSView alloc] initWithFrame:[backgroundView_ frame]]);

  loadingIndicator_.reset(
      [[NSProgressIndicator alloc] initWithFrame:NSZeroRect]);
  [loadingIndicator_ setStyle:NSProgressIndicatorSpinningStyle];
  [loadingIndicator_ sizeToFit];
  NSRect indicatorRect = [loadingIndicator_ frame];
  indicatorRect.origin.x = NSWidth(contentsRect) / 2 - NSMidX(indicatorRect);
  indicatorRect.origin.y = NSHeight(contentsRect) / 2 - NSMidY(indicatorRect);
  [loadingIndicator_ setFrame:indicatorRect];
  [loadingIndicator_ setDisplayedWhenStopped:NO];
  [loadingIndicator_ startAnimation:self];

  if (messageText_) {
    [contentsView_ addSubview:messageTextBackground];

    // Add a scroll view in case the translation is long and doesn't fit. Mac
    // likes to hide scrollbars, so add to the height so the user can see part
    // of the next line of text: just extend out into the padding towards the
    // text background's border. Subtract at least 2: one for the border stroke
    // and one for a bit of padding.
    NSRect frameRect = [messageText_ frame];
    frameRect.size.height += kMessageTextInset - 2;
    messageScrollView_.reset([[NSScrollView alloc] initWithFrame:frameRect]);
    [messageScrollView_ setHasVerticalScroller:YES];
    [messageScrollView_ setAutohidesScrollers:YES];

    // Now the message is going into an NSScrollView, origin should be 0, 0.
    frameRect = [messageText_ frame];
    frameRect.origin = NSMakePoint(0, 0);
    [messageText_ setFrame:frameRect];

    [messageScrollView_ setDocumentView:messageText_];
    [contentsView_ addSubview:messageScrollView_];
  }
  [contentsView_ addSubview:[appsGridController_ view]];
  [contentsView_ addSubview:pagerControl_];
  [contentsView_ addSubview:loadingIndicator_];
  [backgroundView_ addSubview:contentsView_];
  [backgroundView_ addSubview:[appsSearchResultsController_ view]];
  [backgroundView_ addSubview:[appsSearchBoxController_ view]];
  [containerView addSubview:backgroundView_];
  [self setView:containerView];
}

- (void)revealSearchResults:(BOOL)show {
  if (show == showingSearchResults_)
    return;

  showingSearchResults_ = show;
  NSSize contentsSize = [contentsView_ frame].size;
  NSRect resultsTargetRect = NSMakeRect(
      0, kSearchInputHeight + kTopSeparatorSize,
      contentsSize.width, contentsSize.height);
  NSRect contentsTargetRect = resultsTargetRect;

  // Shows results by sliding the grid and pager down to the bottom of the view.
  // Hides results by collapsing the search results container to a height of 0.
  if (show)
    contentsTargetRect.origin.y += NSHeight(contentsTargetRect);
  else
    resultsTargetRect.size.height = 0;

  [[NSAnimationContext currentContext] setDuration:kResultsAnimationDuration];
  [[contentsView_ animator] setFrame:contentsTargetRect];
  [[[appsSearchResultsController_ view] animator] setFrame:resultsTargetRect];
}

- (void)totalPagesChanged {
  size_t pageCount = [appsGridController_ pageCount];
  [pagerControl_ setSegmentCount:pageCount];

  NSRect viewFrame = [[pagerControl_ superview] bounds];
  CGFloat segmentWidth = std::min(
      kMaxSegmentWidth,
      (viewFrame.size.width - 2 * kMinPagerMargin) / pageCount);

  for (size_t i = 0; i < pageCount; ++i) {
    [pagerControl_ setWidth:segmentWidth
                 forSegment:i];
    [[pagerControl_ cell] setTag:i
                      forSegment:i];
  }

  // Center in view.
  [pagerControl_ sizeToFit];
  [pagerControl_ setFrame:
      NSMakeRect(NSMidX(viewFrame) - NSMidX([pagerControl_ bounds]),
                 viewFrame.size.height - kPagerPreferredHeight,
                 [pagerControl_ bounds].size.width,
                 kPagerPreferredHeight)];
}

- (void)selectedPageChanged:(int)newSelected {
  [pagerControl_ selectSegmentWithTag:newSelected];
}

- (void)pageVisibilityChanged {
  [pagerControl_ setNeedsDisplay:YES];
}

- (NSInteger)pagerSegmentAtLocation:(NSPoint)locationInWindow {
  return [pagerControl_ findAndHighlightSegmentAtLocation:locationInWindow];
}

- (app_list::SearchBoxModel*)searchBoxModel {
  app_list::AppListModel* appListModel = [appsGridController_ model];
  return appListModel ? appListModel->search_box() : NULL;
}

- (app_list::AppListViewDelegate*)appListDelegate {
  return [self delegate];
}

- (BOOL)control:(NSControl*)control
               textView:(NSTextView*)textView
    doCommandBySelector:(SEL)command {
  if (showingSearchResults_)
    return [appsSearchResultsController_ handleCommandBySelector:command];

  // If anything has been written, let the search view handle it.
  if ([[control stringValue] length] > 0)
    return NO;

  // Handle escape.
  if (command == @selector(complete:) ||
      command == @selector(cancel:) ||
      command == @selector(cancelOperation:)) {
    if (delegate_)
      delegate_->Dismiss();
    return YES;
  }

  // Possibly handle grid navigation.
  return [appsGridController_ handleCommandBySelector:command];
}

- (void)modelTextDidChange {
  app_list::SearchBoxModel* searchBoxModel = [self searchBoxModel];
  if (!searchBoxModel || !delegate_)
    return;

  base::string16 query;
  base::TrimWhitespace(searchBoxModel->text(), base::TRIM_ALL, &query);
  BOOL shouldShowSearch = !query.empty();
  [self revealSearchResults:shouldShowSearch];
  if (shouldShowSearch)
    delegate_->StartSearch();
  else
    delegate_->StopSearch();
}

- (app_list::AppListModel*)appListModel {
  return [appsGridController_ model];
}

- (void)openResult:(app_list::SearchResult*)result {
  if (delegate_) {
    delegate_->OpenSearchResult(
        result, false /* auto_launch */, 0 /* event flags */);
  }
}

- (void)onProfilesChanged {
  [appsSearchBoxController_ rebuildMenu];
}

// NSTextViewDelegate implementation.

- (BOOL)textView:(NSTextView*)textView
    clickedOnLink:(id)link
          atIndex:(NSUInteger)charIndex {
  DCHECK(delegate_);
  delegate_->OpenLearnMoreLink();
  return YES;
}

- (NSArray*)textView:(NSTextView*)aTextView
    willChangeSelectionFromCharacterRanges:(NSArray*)oldSelectedCharRanges
                         toCharacterRanges:(NSArray*)newSelectedCharRanges {
  return oldSelectedCharRanges;
}

@end
