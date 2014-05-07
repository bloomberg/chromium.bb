// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/app_list/cocoa/apps_grid_view_item.h"

#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "skia/ext/skia_utils_mac.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_item.h"
#include "ui/app_list/app_list_item_observer.h"
#import "ui/app_list/cocoa/apps_grid_controller.h"
#import "ui/base/cocoa/menu_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_util_mac.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"

namespace {

// Padding from the top of the tile to the top of the app icon.
const CGFloat kTileTopPadding = 10;

const CGFloat kIconSize = 48;

const CGFloat kProgressBarHorizontalPadding = 8;
const CGFloat kProgressBarVerticalPadding = 13;

// On Mac, fonts of the same enum from ResourceBundle are larger. The smallest
// enum is already used, so it needs to be reduced further to match Windows.
const int kMacFontSizeDelta = -1;

}  // namespace

@class AppsGridItemBackgroundView;

@interface AppsGridViewItem ()

// Typed accessor for the root view.
- (AppsGridItemBackgroundView*)itemBackgroundView;

// Bridged methods from app_list::AppListItemObserver:
// Update the title, correctly setting the color if the button is highlighted.
- (void)updateButtonTitle;

// Update the button image after ensuring its dimensions are |kIconSize|.
- (void)updateButtonImage;

// Ensure the page this item is on is the visible page in the grid.
- (void)ensureVisible;

// Add or remove a progress bar from the view.
- (void)setItemIsInstalling:(BOOL)isInstalling;

// Update the progress bar to represent |percent|, or make it indeterminate if
// |percent| is -1, when unpacking begins.
- (void)setPercentDownloaded:(int)percent;

@end

namespace app_list {

class ItemModelObserverBridge : public app_list::AppListItemObserver {
 public:
  ItemModelObserverBridge(AppsGridViewItem* parent, AppListItem* model);
  virtual ~ItemModelObserverBridge();

  AppListItem* model() { return model_; }
  NSMenu* GetContextMenu();

  virtual void ItemIconChanged() OVERRIDE;
  virtual void ItemNameChanged() OVERRIDE;
  virtual void ItemHighlightedChanged() OVERRIDE;
  virtual void ItemIsInstallingChanged() OVERRIDE;
  virtual void ItemPercentDownloadedChanged() OVERRIDE;

 private:
  AppsGridViewItem* parent_;  // Weak. Owns us.
  AppListItem* model_;  // Weak. Owned by AppListModel.
  base::scoped_nsobject<MenuController> context_menu_controller_;

  DISALLOW_COPY_AND_ASSIGN(ItemModelObserverBridge);
};

ItemModelObserverBridge::ItemModelObserverBridge(AppsGridViewItem* parent,
                                       AppListItem* model)
    : parent_(parent),
      model_(model) {
  model_->AddObserver(this);
}

ItemModelObserverBridge::~ItemModelObserverBridge() {
  model_->RemoveObserver(this);
}

NSMenu* ItemModelObserverBridge::GetContextMenu() {
  if (!context_menu_controller_) {
    ui::MenuModel* menu_model = model_->GetContextMenuModel();
    if (!menu_model)
      return nil;

    context_menu_controller_.reset(
        [[MenuController alloc] initWithModel:menu_model
                       useWithPopUpButtonCell:NO]);
  }
  return [context_menu_controller_ menu];
}

void ItemModelObserverBridge::ItemIconChanged() {
  [parent_ updateButtonImage];
}

void ItemModelObserverBridge::ItemNameChanged() {
  [parent_ updateButtonTitle];
}

void ItemModelObserverBridge::ItemHighlightedChanged() {
  if (model_->highlighted())
    [parent_ ensureVisible];
}

void ItemModelObserverBridge::ItemIsInstallingChanged() {
  [parent_ setItemIsInstalling:model_->is_installing()];
}

void ItemModelObserverBridge::ItemPercentDownloadedChanged() {
  [parent_ setPercentDownloaded:model_->percent_downloaded()];
}

}  // namespace app_list

// Container for an NSButton to allow proper alignment of the icon in the apps
// grid, and to draw with a highlight when selected.
@interface AppsGridItemBackgroundView : NSView {
 @private
  BOOL selected_;
}

- (NSButton*)button;

- (void)setSelected:(BOOL)flag;

@end

@interface AppsGridItemButtonCell : NSButtonCell {
 @private
  BOOL hasShadow_;
}

@property(assign, nonatomic) BOOL hasShadow;

@end

@interface AppsGridItemButton : NSButton;
@end

@implementation AppsGridItemBackgroundView

- (NSButton*)button {
  // These views are part of a prototype NSCollectionViewItem, copied with an
  // NSCoder. Rather than encoding additional members, the following relies on
  // the button always being the first item added to AppsGridItemBackgroundView.
  return base::mac::ObjCCastStrict<NSButton>([[self subviews] objectAtIndex:0]);
}

- (void)setSelected:(BOOL)flag {
  DCHECK(selected_ != flag);
  selected_ = flag;
  [self setNeedsDisplay:YES];
}

// Ignore all hit tests. The grid controller needs to be the owner of any drags.
- (NSView*)hitTest:(NSPoint)aPoint {
  return nil;
}

- (void)drawRect:(NSRect)dirtyRect {
  if (!selected_)
    return;

  [gfx::SkColorToSRGBNSColor(app_list::kSelectedColor) set];
  NSRectFillUsingOperation(dirtyRect, NSCompositeSourceOver);
}

- (void)mouseDown:(NSEvent*)theEvent {
  [[[self button] cell] setHighlighted:YES];
}

- (void)mouseDragged:(NSEvent*)theEvent {
  NSPoint pointInView = [self convertPoint:[theEvent locationInWindow]
                                  fromView:nil];
  BOOL isInView = [self mouse:pointInView inRect:[self bounds]];
  [[[self button] cell] setHighlighted:isInView];
}

- (void)mouseUp:(NSEvent*)theEvent {
  NSPoint pointInView = [self convertPoint:[theEvent locationInWindow]
                                  fromView:nil];
  if (![self mouse:pointInView inRect:[self bounds]])
    return;

  [[self button] performClick:self];
}

@end

@implementation AppsGridViewItem

- (id)initWithSize:(NSSize)tileSize {
  if ((self = [super init])) {
    base::scoped_nsobject<AppsGridItemButton> prototypeButton(
        [[AppsGridItemButton alloc] initWithFrame:NSMakeRect(
            0, 0, tileSize.width, tileSize.height - kTileTopPadding)]);

    // This NSButton style always positions the icon at the very top of the
    // button frame. AppsGridViewItem uses an enclosing view so that it is
    // visually correct.
    [prototypeButton setImagePosition:NSImageAbove];
    [prototypeButton setButtonType:NSMomentaryChangeButton];
    [prototypeButton setBordered:NO];

    base::scoped_nsobject<AppsGridItemBackgroundView> prototypeButtonBackground(
        [[AppsGridItemBackgroundView alloc]
            initWithFrame:NSMakeRect(0, 0, tileSize.width, tileSize.height)]);
    [prototypeButtonBackground addSubview:prototypeButton];
    [self setView:prototypeButtonBackground];
  }
  return self;
}

- (NSProgressIndicator*)progressIndicator {
  return progressIndicator_;
}

- (void)updateButtonTitle {
  if (progressIndicator_)
    return;

  base::scoped_nsobject<NSMutableParagraphStyle> paragraphStyle(
      [[NSMutableParagraphStyle alloc] init]);
  [paragraphStyle setLineBreakMode:NSLineBreakByTruncatingTail];
  [paragraphStyle setAlignment:NSCenterTextAlignment];
  NSDictionary* titleAttributes = @{
    NSParagraphStyleAttributeName : paragraphStyle,
    NSFontAttributeName : ui::ResourceBundle::GetSharedInstance()
        .GetFontList(app_list::kItemTextFontStyle)
        .DeriveWithSizeDelta(kMacFontSizeDelta)
        .GetPrimaryFont()
        .GetNativeFont(),
    NSForegroundColorAttributeName : [self isSelected] ?
        gfx::SkColorToSRGBNSColor(app_list::kGridTitleHoverColor) :
        gfx::SkColorToSRGBNSColor(app_list::kGridTitleColor)
  };
  NSString* buttonTitle =
      base::SysUTF8ToNSString([self model]->GetDisplayName());
  base::scoped_nsobject<NSAttributedString> attributedTitle(
      [[NSAttributedString alloc] initWithString:buttonTitle
                                      attributes:titleAttributes]);
  [[self button] setAttributedTitle:attributedTitle];

  // If the display name would be truncated in the NSButton, or if the display
  // name differs from the full name, add a tooltip showing the full name.
  NSRect titleRect =
      [[[self button] cell] titleRectForBounds:[[self button] bounds]];
  if ([self model]->name() == [self model]->GetDisplayName() &&
      [attributedTitle size].width < NSWidth(titleRect)) {
    [[self view] removeAllToolTips];
  } else {
    [[self view] setToolTip:base::SysUTF8ToNSString([self model]->name())];
  }
}

- (void)updateButtonImage {
  const gfx::Size iconSize = gfx::Size(kIconSize, kIconSize);
  gfx::ImageSkia icon = [self model]->icon();
  if (icon.size() != iconSize) {
    icon = gfx::ImageSkiaOperations::CreateResizedImage(
        icon, skia::ImageOperations::RESIZE_BEST, iconSize);
  }
  NSImage* buttonImage = gfx::NSImageFromImageSkiaWithColorSpace(
      icon, base::mac::GetSRGBColorSpace());
  [[self button] setImage:buttonImage];
  [[[self button] cell] setHasShadow:[self model]->has_shadow()];
}

- (void)setModel:(app_list::AppListItem*)itemModel {
  [trackingArea_.get() clearOwner];
  if (!itemModel) {
    observerBridge_.reset();
    return;
  }

  observerBridge_.reset(new app_list::ItemModelObserverBridge(self, itemModel));
  [self updateButtonTitle];
  [self updateButtonImage];

  if (trackingArea_.get())
    [[self view] removeTrackingArea:trackingArea_.get()];

  trackingArea_.reset(
      [[CrTrackingArea alloc] initWithRect:NSZeroRect
                                   options:NSTrackingInVisibleRect |
                                           NSTrackingMouseEnteredAndExited |
                                           NSTrackingActiveInKeyWindow
                                     owner:self
                                  userInfo:nil]);
  [[self view] addTrackingArea:trackingArea_.get()];
}

- (app_list::AppListItem*)model {
  return observerBridge_->model();
}

- (NSButton*)button {
  return [[self itemBackgroundView] button];
}

- (NSMenu*)contextMenu {
  // Don't show the menu if button is already held down, e.g. with a left-click.
  if ([[[self button] cell] isHighlighted])
    return nil;

  [self setSelected:YES];
  return observerBridge_->GetContextMenu();
}

- (NSBitmapImageRep*)dragRepresentationForRestore:(BOOL)isRestore {
  NSButton* button = [self button];
  NSView* itemView = [self view];

  // The snapshot is never drawn as if it was selected. Also remove the cell
  // highlight on the button image, added when it was clicked.
  [button setHidden:NO];
  [[button cell] setHighlighted:NO];
  [self setSelected:NO];
  [progressIndicator_ setHidden:YES];
  if (isRestore)
    [self updateButtonTitle];
  else
    [button setTitle:@""];

  NSBitmapImageRep* imageRep =
      [itemView bitmapImageRepForCachingDisplayInRect:[itemView visibleRect]];
  [itemView cacheDisplayInRect:[itemView visibleRect]
              toBitmapImageRep:imageRep];

  if (isRestore) {
    [progressIndicator_ setHidden:NO];
    [self setSelected:YES];
  }
  // Button is always hidden until the drag animation completes.
  [button setHidden:YES];
  return imageRep;
}

- (void)ensureVisible {
  NSCollectionView* collectionView = [self collectionView];
  AppsGridController* gridController =
      base::mac::ObjCCastStrict<AppsGridController>([collectionView delegate]);
  size_t pageIndex = [gridController pageIndexForCollectionView:collectionView];
  [gridController scrollToPage:pageIndex];
}

- (void)setItemIsInstalling:(BOOL)isInstalling {
  if (!isInstalling == !progressIndicator_)
    return;

  [self ensureVisible];
  if (!isInstalling) {
    [progressIndicator_ removeFromSuperview];
    progressIndicator_.reset();
    [self updateButtonTitle];
    [self setSelected:YES];
    return;
  }

  NSRect rect = NSMakeRect(
      kProgressBarHorizontalPadding,
      kProgressBarVerticalPadding,
      NSWidth([[self view] bounds]) - 2 * kProgressBarHorizontalPadding,
      NSProgressIndicatorPreferredAquaThickness);
  [[self button] setTitle:@""];
  progressIndicator_.reset([[NSProgressIndicator alloc] initWithFrame:rect]);
  [progressIndicator_ setIndeterminate:NO];
  [progressIndicator_ setControlSize:NSSmallControlSize];
  [[self view] addSubview:progressIndicator_];
}

- (void)setPercentDownloaded:(int)percent {
  // In a corner case, items can be installing when they are first added. For
  // those, the icon will start desaturated. Wait for a progress update before
  // showing the progress bar.
  [self setItemIsInstalling:YES];
  if (percent != -1) {
    [progressIndicator_ setDoubleValue:percent];
    return;
  }

  // Otherwise, fully downloaded and waiting for install to complete.
  [progressIndicator_ setIndeterminate:YES];
  [progressIndicator_ startAnimation:self];
}

- (AppsGridItemBackgroundView*)itemBackgroundView {
  return base::mac::ObjCCastStrict<AppsGridItemBackgroundView>([self view]);
}

- (void)mouseEntered:(NSEvent*)theEvent {
  [self setSelected:YES];
}

- (void)mouseExited:(NSEvent*)theEvent {
  [self setSelected:NO];
}

- (void)setSelected:(BOOL)flag {
  if ([self isSelected] == flag)
    return;

  [[self itemBackgroundView] setSelected:flag];
  [super setSelected:flag];
  [self updateButtonTitle];
}

@end

@implementation AppsGridItemButton

+ (Class)cellClass {
  return [AppsGridItemButtonCell class];
}

@end

@implementation AppsGridItemButtonCell

@synthesize hasShadow = hasShadow_;

- (void)drawImage:(NSImage*)image
        withFrame:(NSRect)frame
           inView:(NSView*)controlView {
  if (!hasShadow_) {
    [super drawImage:image
           withFrame:frame
              inView:controlView];
    return;
  }

  base::scoped_nsobject<NSShadow> shadow([[NSShadow alloc] init]);
  gfx::ScopedNSGraphicsContextSaveGState context;
  [shadow setShadowOffset:NSMakeSize(0, -2)];
  [shadow setShadowBlurRadius:2.0];
  [shadow setShadowColor:[NSColor colorWithCalibratedWhite:0
                                                     alpha:0.14]];
  [shadow set];

  [super drawImage:image
         withFrame:frame
            inView:controlView];
}

// Workaround for http://crbug.com/324365: AppKit in Mavericks tries to call
// - [NSButtonCell item] when inspecting accessibility. Without this, an
// unrecognized selector exception is thrown inside AppKit, crashing Chrome.
- (id)item {
  return nil;
}

@end
