// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/cocoa/menu_controller.h"

#include "base/cancelable_callback.h"
#include "base/logging.h"
#include "base/mac/bind_objc_block.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/platform_accelerator_cocoa.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/models/simple_menu_model.h"
#import "ui/events/event_utils.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/text_elider.h"
#include "ui/strings/grit/ui_strings.h"

namespace {

// Called when an empty submenu is created. This inserts a menu item labeled
// "(empty)" into the submenu. Matches Windows behavior.
NSMenu* MakeEmptySubmenu() {
  base::scoped_nsobject<NSMenu> submenu([[NSMenu alloc] initWithTitle:@""]);
  NSString* empty_menu_title =
      l10n_util::GetNSString(IDS_APP_MENU_EMPTY_SUBMENU);
  [submenu addItemWithTitle:empty_menu_title action:NULL keyEquivalent:@""];
  [[submenu itemAtIndex:0] setEnabled:NO];
  return submenu.autorelease();
}

// Called when adding a submenu to the menu and checks if the submenu, via its
// |model|, has visible child items.
bool MenuHasVisibleItems(const ui::MenuModel* model) {
  int count = model->GetItemCount();
  for (int index = 0; index < count; index++) {
    if (model->IsVisibleAt(index))
      return true;
  }
  return false;
}

}  // namespace

NSString* const kMenuControllerMenuWillOpenNotification =
    @"MenuControllerMenuWillOpen";
NSString* const kMenuControllerMenuDidCloseNotification =
    @"MenuControllerMenuDidClose";

// Internal methods.
@interface MenuControllerCocoa ()
// Adds a separator item at the given index. As the separator doesn't need
// anything from the model, this method doesn't need the model index as the
// other method below does.
- (void)addSeparatorToMenu:(NSMenu*)menu atIndex:(int)index;

// Called via a private API hook shortly after the event that selects a menu
// item arrives.
- (void)itemWillBeSelected:(NSMenuItem*)sender;

// Called when the user chooses a particular menu item. AppKit sends this only
// after the menu has fully faded out. |sender| is the menu item chosen.
- (void)itemSelected:(id)sender;

// Called by the posted task to selected an item during menu fade out.
// |uiEventFlags| are the ui::EventFlags captured from the triggering NSEvent.
- (void)itemSelected:(id)sender uiEventFlags:(int)uiEventFlags;
@end

@interface ResponsiveNSMenuItem : NSMenuItem
@end

@implementation MenuControllerCocoa {
  BOOL useWithPopUpButtonCell_;  // If YES, 0th item is blank
  BOOL isMenuOpen_;
  BOOL postItemSelectedAsTask_;
  std::unique_ptr<base::CancelableClosure> postedItemSelectedTask_;
}

@synthesize model = model_;
@synthesize useWithPopUpButtonCell = useWithPopUpButtonCell_;
@synthesize postItemSelectedAsTask = postItemSelectedAsTask_;

+ (base::string16)elideMenuTitle:(const base::string16&)title
                         toWidth:(int)width {
  NSFont* nsfont = [NSFont menuBarFontOfSize:0];  // 0 means "default"
  return gfx::ElideText(title, gfx::FontList(gfx::Font(nsfont)), width,
                        gfx::ELIDE_TAIL, gfx::Typesetter::NATIVE);
}

- (id)init {
  self = [super init];
  return self;
}

- (id)initWithModel:(ui::MenuModel*)model
    useWithPopUpButtonCell:(BOOL)useWithCell {
  if ((self = [super init])) {
    model_ = model;
    useWithPopUpButtonCell_ = useWithCell;
    [self menu];
  }
  return self;
}

- (void)dealloc {
  [menu_ setDelegate:nil];

  // Close the menu if it is still open. This could happen if a tab gets closed
  // while its context menu is still open.
  [self cancel];

  model_ = NULL;
  [super dealloc];
}

- (void)cancel {
  if (isMenuOpen_) {
    [menu_ cancelTracking];
    model_->MenuWillClose();
    isMenuOpen_ = NO;
  }
}

- (NSMenu*)menuFromModel:(ui::MenuModel*)model {
  NSMenu* menu = [[[NSMenu alloc] initWithTitle:@""] autorelease];

  const int count = model->GetItemCount();
  for (int index = 0; index < count; index++) {
    if (model->GetTypeAt(index) == ui::MenuModel::TYPE_SEPARATOR)
      [self addSeparatorToMenu:menu atIndex:index];
    else
      [self addItemToMenu:menu atIndex:index fromModel:model];
  }

  return menu;
}

- (int)maxWidthForMenuModel:(ui::MenuModel*)model
                 modelIndex:(int)modelIndex {
  return -1;
}

- (void)addSeparatorToMenu:(NSMenu*)menu
                   atIndex:(int)index {
  NSMenuItem* separator = [NSMenuItem separatorItem];
  [menu insertItem:separator atIndex:index];
}

- (void)addItemToMenu:(NSMenu*)menu
              atIndex:(NSInteger)index
            fromModel:(ui::MenuModel*)model {
  base::string16 label16 = model->GetLabelAt(index);
  int maxWidth = [self maxWidthForMenuModel:model modelIndex:index];
  if (maxWidth != -1)
    label16 = [MenuControllerCocoa elideMenuTitle:label16 toWidth:maxWidth];

  NSString* label = l10n_util::FixUpWindowsStyleLabel(label16);
  base::scoped_nsobject<NSMenuItem> item([[ResponsiveNSMenuItem alloc]
      initWithTitle:label
             action:@selector(itemSelected:)
      keyEquivalent:@""]);

  // If the menu item has an icon, set it.
  gfx::Image icon;
  if (model->GetIconAt(index, &icon) && !icon.IsEmpty())
    [item setImage:icon.ToNSImage()];

  ui::MenuModel::ItemType type = model->GetTypeAt(index);
  if (type == ui::MenuModel::TYPE_SUBMENU && model->IsVisibleAt(index)) {
    ui::MenuModel* submenuModel = model->GetSubmenuModelAt(index);

    // If there are visible items, recursively build the submenu.
    NSMenu* submenu = MenuHasVisibleItems(submenuModel)
                          ? [self menuFromModel:submenuModel]
                          : MakeEmptySubmenu();

    [item setTarget:nil];
    [item setAction:nil];
    [item setSubmenu:submenu];
  } else {
    // The MenuModel works on indexes so we can't just set the command id as the
    // tag like we do in other menus. Also set the represented object to be
    // the model so hierarchical menus check the correct index in the correct
    // model. Setting the target to |self| allows this class to participate
    // in validation of the menu items.
    [item setTag:index];
    [item setTarget:self];
    NSValue* modelObject = [NSValue valueWithPointer:model];
    [item setRepresentedObject:modelObject];  // Retains |modelObject|.
    ui::Accelerator accelerator;
    if (model->GetAcceleratorAt(index, &accelerator)) {
      const ui::PlatformAcceleratorCocoa* platformAccelerator =
          static_cast<const ui::PlatformAcceleratorCocoa*>(
              accelerator.platform_accelerator());
      if (platformAccelerator) {
        [item setKeyEquivalent:platformAccelerator->characters()];
        [item setKeyEquivalentModifierMask:
            platformAccelerator->modifier_mask()];
      }
    }
  }
  [menu insertItem:item atIndex:index];
}

- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item {
  SEL action = [item action];
  if (action != @selector(itemSelected:))
    return NO;

  NSInteger modelIndex = [item tag];
  ui::MenuModel* model =
      static_cast<ui::MenuModel*>(
          [[(id)item representedObject] pointerValue]);
  DCHECK(model);
  if (model) {
    BOOL checked = model->IsItemCheckedAt(modelIndex);
    DCHECK([(id)item isKindOfClass:[NSMenuItem class]]);
    [(id)item setState:(checked ? NSOnState : NSOffState)];
    [(id)item setHidden:(!model->IsVisibleAt(modelIndex))];
    if (model->IsItemDynamicAt(modelIndex)) {
      // Update the label and the icon.
      NSString* label =
          l10n_util::FixUpWindowsStyleLabel(model->GetLabelAt(modelIndex));
      [(id)item setTitle:label];

      gfx::Image icon;
      model->GetIconAt(modelIndex, &icon);
      [(id)item setImage:icon.IsEmpty() ? nil : icon.ToNSImage()];
    }
    const gfx::FontList* font_list = model->GetLabelFontListAt(modelIndex);
    if (font_list) {
      NSDictionary *attributes =
          [NSDictionary dictionaryWithObject:font_list->GetPrimaryFont().
                                             GetNativeFont()
                                      forKey:NSFontAttributeName];
      base::scoped_nsobject<NSAttributedString> title(
          [[NSAttributedString alloc] initWithString:[(id)item title]
                                          attributes:attributes]);
      [(id)item setAttributedTitle:title.get()];
    }
    return model->IsEnabledAt(modelIndex);
  }
  return NO;
}

- (void)itemWillBeSelected:(NSMenuItem*)sender {
  if (postItemSelectedAsTask_ && [sender action] == @selector(itemSelected:) &&
      [[sender target]
          respondsToSelector:@selector(itemSelected:uiEventFlags:)]) {
    const int uiEventFlags = ui::EventFlagsFromNative([NSApp currentEvent]);

    // Take care here to retain |menu_| in the block, but not |self|. Since the
    // block may run before -menuDidClose:, a release of the MenuControllerCocoa
    // will think the menu is open, and invoke -cancel. So if the delegate is
    // bad (see below), and decides to release the MenuControllerCocoa in its
    // menu action, ensure the -dealloc happens there. To do otherwise risks
    // |model_| being deleted when it is used in -cancel, whereas that is less
    // likely if the -cancel happens in the delegate method.
    NSMenu* menu = menu_;

    postedItemSelectedTask_ =
        std::make_unique<base::CancelableClosure>(base::BindBlock(^{
          id target = [sender target];
          if ([target respondsToSelector:@selector(itemSelected:uiEventFlags:)])
            [target itemSelected:sender uiEventFlags:uiEventFlags];
          else
            NOTREACHED();

          // Ensure consumers that use -postItemSelectedAsTask:YES have not
          // destroyed the MenuControllerCocoa in the menu action. AppKit will
          // still send messages to [item target] (the MenuControllerCocoa), and
          // the target can not be set to nil here since that prevents re-use of
          // the menu for well-behaved consumers.
          CHECK([menu delegate]);  // Note: set to nil in -dealloc.
        }));
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, postedItemSelectedTask_->callback());
  }
}

- (void)itemSelected:(id)sender {
  // A task created in -itemWillBeSelected: may or may not have run. If not, put
  // it on the stack before running it, in case it destroys |self|.
  if (auto pendingTask = std::move(postedItemSelectedTask_)) {
    if (!pendingTask->IsCancelled())
      pendingTask->callback().Run();
  } else {
    [self itemSelected:sender
          uiEventFlags:ui::EventFlagsFromNative([NSApp currentEvent])];
  }
}

- (void)itemSelected:(id)sender uiEventFlags:(int)uiEventFlags {
  // Cancel any posted task, but don't reset it, so that the correct path is
  // taken in -itemSelected:.
  if (postedItemSelectedTask_)
    postedItemSelectedTask_->Cancel();

  NSInteger modelIndex = [sender tag];
  ui::MenuModel* model =
      static_cast<ui::MenuModel*>(
          [[sender representedObject] pointerValue]);
  DCHECK(model);
  if (model)
    model->ActivatedAt(modelIndex, uiEventFlags);
  // Note: |self| may be destroyed by the call to ActivatedAt().
}

- (NSMenu*)menu {
  if (!menu_ && model_) {
    menu_.reset([[self menuFromModel:model_] retain]);
    [menu_ setDelegate:self];
    // If this is to be used with a NSPopUpButtonCell, add an item at the 0th
    // position that's empty. Doing it after the menu has been constructed won't
    // complicate creation logic, and since the tags are model indexes, they
    // are unaffected by the extra item.
    if (useWithPopUpButtonCell_) {
      base::scoped_nsobject<NSMenuItem> blankItem(
          [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""]);
      [menu_ insertItem:blankItem atIndex:0];
    }
  }
  return menu_.get();
}

- (BOOL)isMenuOpen {
  return isMenuOpen_;
}

- (void)menuWillOpen:(NSMenu*)menu {
  isMenuOpen_ = YES;
  model_->MenuWillShow();
  [[NSNotificationCenter defaultCenter]
      postNotificationName:kMenuControllerMenuWillOpenNotification
                    object:self];
}

- (void)menuDidClose:(NSMenu*)menu {
  if (isMenuOpen_) {
    model_->MenuWillClose();
    isMenuOpen_ = NO;
  }
  [[NSNotificationCenter defaultCenter]
      postNotificationName:kMenuControllerMenuDidCloseNotification
                    object:self];
}

@end

@interface NSMenuItem (Private)
// Private method which is invoked very soon after the event that activates a
// menu item is received. AppKit then spends 300ms or so flashing the menu item,
// and fading out the menu, in private run loop modes.
- (void)_sendItemSelectedNote;
@end

@implementation ResponsiveNSMenuItem
- (void)_sendItemSelectedNote {
  if ([[self target] respondsToSelector:@selector(itemWillBeSelected:)])
    [[self target] itemWillBeSelected:self];
  [super _sendItemSelectedNote];
}
@end
