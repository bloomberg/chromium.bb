// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/app_list/cocoa/apps_search_box_controller.h"

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#import "third_party/GTM/AppKit/GTMNSBezierPath+RoundRect.h"
#include "ui/app_list/search_box_model.h"
#include "ui/app_list/search_box_model_observer.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia_util_mac.h"

namespace {

// Padding either side of the search icon.
const CGFloat kPadding = 14;

// Size of the search icon.
const CGFloat kSearchIconDimension = 32;

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
  [[parent_ searchImageView]
      setImage:gfx::NSImageFromImageSkia(GetModel()->icon())];
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

@implementation AppsSearchBoxController

@synthesize delegate = delegate_;

- (id)initWithFrame:(NSRect)frame {
  if ((self = [super init])) {
    scoped_nsobject<NSView> containerView([[NSView alloc] initWithFrame:frame]);
    [self setView:containerView];
    [self addSubviews];
  }
  return self;
}

- (void)clearSearch {
  [searchTextField_ setStringValue:@""];
  [self controlTextDidChange:nil];
}

- (void)setDelegate:(id<AppsSearchBoxDelegate>)delegate {
  bridge_.reset();  // Ensure observers are cleared before updating |delegate_|.
  delegate_ = delegate;
  if (!delegate_)
    return;

  bridge_.reset(new app_list::SearchBoxModelObserverBridge(self));
}

- (NSTextField*)searchTextField {
  return searchTextField_;
}

- (NSImageView*)searchImageView {
  return searchImageView_;
}

- (void)addSubviews {
  NSRect viewBounds = [[self view] bounds];
  searchImageView_.reset([[NSImageView alloc] initWithFrame:NSMakeRect(
      kPadding, 0, kSearchIconDimension, NSHeight(viewBounds))]);

  searchTextField_.reset([[SearchTextField alloc] initWithFrame:viewBounds]);
  [searchTextField_ setDelegate:self];
  [searchTextField_ setFont:ui::ResourceBundle::GetSharedInstance().GetFont(
      ui::ResourceBundle::MediumFont).GetNativeFont()];
  [searchTextField_
      setMarginsWithLeftMargin:NSMaxX([searchImageView_ frame]) + kPadding
                   rightMargin:kPadding];

  [[self view] addSubview:searchImageView_];
  [[self view] addSubview:searchTextField_];
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
