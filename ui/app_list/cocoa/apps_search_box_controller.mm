// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/app_list/cocoa/apps_search_box_controller.h"

#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/strings/sys_string_conversions.h"
#include "grit/ui_resources.h"
#import "third_party/google_toolbox_for_mac/src/AppKit/GTMNSBezierPath+RoundRect.h"
#include "ui/app_list/app_list_menu.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/search_box_model.h"
#include "ui/app_list/search_box_model_observer.h"
#import "ui/base/cocoa/controls/hover_image_menu_button.h"
#import "ui/base/cocoa/controls/hover_image_menu_button_cell.h"
#import "ui/base/cocoa/menu_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia_util_mac.h"

namespace {

// Padding either side of the search icon and menu button.
const CGFloat kPadding = 14;

// Size of the search icon.
const CGFloat kSearchIconDimension = 32;

// Size of the menu button on the right.
const CGFloat kMenuButtonDimension = 29;

// Menu offset relative to the bottom-right corner of the menu button.
const CGFloat kMenuYOffsetFromButton = -4;
const CGFloat kMenuXOffsetFromButton = -7;

}

@interface AppsSearchBoxController ()

- (NSImageView*)searchImageView;
- (void)addSubviews;

@end

namespace app_list {

class SearchBoxModelObserverBridge : public SearchBoxModelObserver {
 public:
  SearchBoxModelObserverBridge(AppsSearchBoxController* parent);
  virtual ~SearchBoxModelObserverBridge();

  void SetSearchText(const base::string16& text);

  virtual void IconChanged() OVERRIDE;
  virtual void SpeechRecognitionButtonPropChanged() OVERRIDE;
  virtual void HintTextChanged() OVERRIDE;
  virtual void SelectionModelChanged() OVERRIDE;
  virtual void TextChanged() OVERRIDE;

 private:
  SearchBoxModel* GetModel();

  AppsSearchBoxController* parent_;  // Weak. Owns us.

  DISALLOW_COPY_AND_ASSIGN(SearchBoxModelObserverBridge);
};

SearchBoxModelObserverBridge::SearchBoxModelObserverBridge(
    AppsSearchBoxController* parent)
    : parent_(parent) {
  IconChanged();
  HintTextChanged();
  GetModel()->AddObserver(this);
}

SearchBoxModelObserverBridge::~SearchBoxModelObserverBridge() {
  GetModel()->RemoveObserver(this);
}

SearchBoxModel* SearchBoxModelObserverBridge::GetModel() {
  SearchBoxModel* searchBoxModel = [[parent_ delegate] searchBoxModel];
  DCHECK(searchBoxModel);
  return searchBoxModel;
}

void SearchBoxModelObserverBridge::SetSearchText(const base::string16& text) {
  SearchBoxModel* model = GetModel();
  model->RemoveObserver(this);
  model->SetText(text);
  // TODO(tapted): See if this should call SetSelectionModel here.
  model->AddObserver(this);
}

void SearchBoxModelObserverBridge::IconChanged() {
  [[parent_ searchImageView] setImage:gfx::NSImageFromImageSkiaWithColorSpace(
      GetModel()->icon(), base::mac::GetSRGBColorSpace())];
}

void SearchBoxModelObserverBridge::SpeechRecognitionButtonPropChanged() {
  // TODO(mukai): implement.
  NOTIMPLEMENTED();
}

void SearchBoxModelObserverBridge::HintTextChanged() {
  [[[parent_ searchTextField] cell] setPlaceholderString:
      base::SysUTF16ToNSString(GetModel()->hint_text())];
}

void SearchBoxModelObserverBridge::SelectionModelChanged() {
  // TODO(tapted): See if anything needs to be done here for RTL.
}

void SearchBoxModelObserverBridge::TextChanged() {
  // Currently the model text is only changed when we are not observing it, or
  // it is changed in tests to establish a particular state.
  [[parent_ searchTextField]
      setStringValue:base::SysUTF16ToNSString(GetModel()->text())];
  [[parent_ delegate] modelTextDidChange];
}

}  // namespace app_list

@interface SearchTextField : NSTextField {
 @private
  NSRect textFrameInset_;
}

@property(readonly, nonatomic) NSRect textFrameInset;

- (void)setMarginsWithLeftMargin:(CGFloat)leftMargin
                     rightMargin:(CGFloat)rightMargin;

@end

@interface AppListMenuController : MenuController {
 @private
  AppsSearchBoxController* searchBoxController_;  // Weak. Owns us.
}

- (id)initWithSearchBoxController:(AppsSearchBoxController*)parent;

@end

@implementation AppsSearchBoxController

@synthesize delegate = delegate_;

- (id)initWithFrame:(NSRect)frame {
  if ((self = [super init])) {
    base::scoped_nsobject<NSView> containerView(
        [[NSView alloc] initWithFrame:frame]);
    [self setView:containerView];
    [self addSubviews];
  }
  return self;
}

- (void)clearSearch {
  [searchTextField_ setStringValue:@""];
  [self controlTextDidChange:nil];
}

- (void)rebuildMenu {
  if (![delegate_ appListDelegate])
    return;

  menuController_.reset();
  appListMenu_.reset(
      new app_list::AppListMenu([delegate_ appListDelegate]));
  menuController_.reset([[AppListMenuController alloc]
      initWithSearchBoxController:self]);
  [menuButton_ setMenu:[menuController_ menu]];  // Menu will populate here.
}

- (void)setDelegate:(id<AppsSearchBoxDelegate>)delegate {
  [[menuButton_ menu] removeAllItems];
  menuController_.reset();
  appListMenu_.reset();
  bridge_.reset();  // Ensure observers are cleared before updating |delegate_|.
  delegate_ = delegate;
  if (!delegate_)
    return;

  bridge_.reset(new app_list::SearchBoxModelObserverBridge(self));
  [self rebuildMenu];
}

- (NSTextField*)searchTextField {
  return searchTextField_;
}

- (NSPopUpButton*)menuControl {
  return menuButton_;
}

- (app_list::AppListMenu*)appListMenu {
  return appListMenu_.get();
}

- (NSImageView*)searchImageView {
  return searchImageView_;
}

- (void)addSubviews {
  NSRect viewBounds = [[self view] bounds];
  searchImageView_.reset([[NSImageView alloc] initWithFrame:NSMakeRect(
      kPadding, 0, kSearchIconDimension, NSHeight(viewBounds))]);

  searchTextField_.reset([[SearchTextField alloc] initWithFrame:viewBounds]);
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  [searchTextField_ setDelegate:self];
  [searchTextField_ setFont:rb.GetFont(
      ui::ResourceBundle::MediumFont).GetNativeFont()];
  [searchTextField_
      setMarginsWithLeftMargin:NSMaxX([searchImageView_ frame]) + kPadding
                   rightMargin:kMenuButtonDimension + 2 * kPadding];

  // Add the drop-down menu, with a custom button.
  NSRect buttonFrame = NSMakeRect(
      NSWidth(viewBounds) - kMenuButtonDimension - kPadding,
      floor(NSMidY(viewBounds) - kMenuButtonDimension / 2),
      kMenuButtonDimension,
      kMenuButtonDimension);
  menuButton_.reset([[HoverImageMenuButton alloc] initWithFrame:buttonFrame
                                                      pullsDown:YES]);
  [[menuButton_ hoverImageMenuButtonCell] setDefaultImage:
      rb.GetNativeImageNamed(IDR_APP_LIST_TOOLS_NORMAL).AsNSImage()];
  [[menuButton_ hoverImageMenuButtonCell] setAlternateImage:
      rb.GetNativeImageNamed(IDR_APP_LIST_TOOLS_PRESSED).AsNSImage()];
  [[menuButton_ hoverImageMenuButtonCell] setHoverImage:
      rb.GetNativeImageNamed(IDR_APP_LIST_TOOLS_HOVER).AsNSImage()];

  [[self view] addSubview:searchImageView_];
  [[self view] addSubview:searchTextField_];
  [[self view] addSubview:menuButton_];
}

- (BOOL)control:(NSControl*)control
               textView:(NSTextView*)textView
    doCommandBySelector:(SEL)command {
  // Forward the message first, to handle grid or search results navigation.
  BOOL handled = [delegate_ control:control
                           textView:textView
                doCommandBySelector:command];
  if (handled)
    return YES;

  // If the delegate did not handle the escape key, it means the window was not
  // dismissed because there were search results. Clear them.
  if (command == @selector(complete:)) {
    [self clearSearch];
    return YES;
  }

  return NO;
}

- (void)controlTextDidChange:(NSNotification*)notification {
  if (bridge_) {
    bridge_->SetSearchText(
        base::SysNSStringToUTF16([searchTextField_ stringValue]));
  }

  [delegate_ modelTextDidChange];
}

@end

@interface SearchTextFieldCell : NSTextFieldCell;

- (NSRect)textFrameForFrameInternal:(NSRect)cellFrame;

@end

@implementation SearchTextField

@synthesize textFrameInset = textFrameInset_;

+ (Class)cellClass {
  return [SearchTextFieldCell class];
}

- (id)initWithFrame:(NSRect)theFrame {
  if ((self = [super initWithFrame:theFrame])) {
    [self setFocusRingType:NSFocusRingTypeNone];
    [self setDrawsBackground:NO];
    [self setBordered:NO];
  }
  return self;
}

- (void)setMarginsWithLeftMargin:(CGFloat)leftMargin
                     rightMargin:(CGFloat)rightMargin {
  // Find the preferred height for the current text properties, and center.
  NSRect viewBounds = [self bounds];
  [self sizeToFit];
  NSRect textBounds = [self bounds];
  textFrameInset_.origin.x = leftMargin;
  textFrameInset_.origin.y = floor(NSMidY(viewBounds) - NSMidY(textBounds));
  textFrameInset_.size.width = leftMargin + rightMargin;
  textFrameInset_.size.height = NSHeight(viewBounds) - NSHeight(textBounds);
  [self setFrame:viewBounds];
}

@end

@implementation SearchTextFieldCell

- (NSRect)textFrameForFrameInternal:(NSRect)cellFrame {
  SearchTextField* searchTextField =
      base::mac::ObjCCastStrict<SearchTextField>([self controlView]);
  NSRect insetRect = [searchTextField textFrameInset];
  cellFrame.origin.x += insetRect.origin.x;
  cellFrame.origin.y += insetRect.origin.y;
  cellFrame.size.width -= insetRect.size.width;
  cellFrame.size.height -= insetRect.size.height;
  return cellFrame;
}

- (NSRect)textFrameForFrame:(NSRect)cellFrame {
  return [self textFrameForFrameInternal:cellFrame];
}

- (NSRect)textCursorFrameForFrame:(NSRect)cellFrame {
  return [self textFrameForFrameInternal:cellFrame];
}

- (void)resetCursorRect:(NSRect)cellFrame
                 inView:(NSView*)controlView {
  [super resetCursorRect:[self textCursorFrameForFrame:cellFrame]
                  inView:controlView];
}

- (NSRect)drawingRectForBounds:(NSRect)theRect {
  return [super drawingRectForBounds:[self textFrameForFrame:theRect]];
}

- (void)editWithFrame:(NSRect)cellFrame
               inView:(NSView*)controlView
               editor:(NSText*)editor
             delegate:(id)delegate
                event:(NSEvent*)event {
  [super editWithFrame:[self textFrameForFrame:cellFrame]
                inView:controlView
                editor:editor
              delegate:delegate
                 event:event];
}

- (void)selectWithFrame:(NSRect)cellFrame
                 inView:(NSView*)controlView
                 editor:(NSText*)editor
               delegate:(id)delegate
                  start:(NSInteger)start
                 length:(NSInteger)length {
  [super selectWithFrame:[self textFrameForFrame:cellFrame]
                  inView:controlView
                  editor:editor
                delegate:delegate
                   start:start
                  length:length];
}

@end

@implementation AppListMenuController

- (id)initWithSearchBoxController:(AppsSearchBoxController*)parent {
  // Need to initialze super with a NULL model, otherwise it will immediately
  // try to populate, which can't be done until setting the parent.
  if ((self = [super initWithModel:NULL
            useWithPopUpButtonCell:YES])) {
    searchBoxController_ = parent;
    [super setModel:[parent appListMenu]->menu_model()];
  }
  return self;
}

- (NSRect)confinementRectForMenu:(NSMenu*)menu
                        onScreen:(NSScreen*)screen {
  NSPopUpButton* menuButton = [searchBoxController_ menuControl];
  // Ensure the menu comes up below the menu button by trimming the window frame
  // to a point anchored below the bottom right of the button.
  NSRect anchorRect = [menuButton convertRect:[menuButton bounds]
                                       toView:nil];
  NSPoint anchorPoint = [[menuButton window] convertBaseToScreen:NSMakePoint(
      NSMaxX(anchorRect) + kMenuXOffsetFromButton,
      NSMinY(anchorRect) - kMenuYOffsetFromButton)];
  NSRect confinementRect = [[menuButton window] frame];
  confinementRect.size = NSMakeSize(anchorPoint.x - NSMinX(confinementRect),
                                    anchorPoint.y - NSMinY(confinementRect));
  return confinementRect;
}

@end
