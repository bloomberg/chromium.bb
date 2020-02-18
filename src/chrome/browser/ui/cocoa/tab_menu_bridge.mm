// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/tab_menu_bridge.h"

#import <Cocoa/Cocoa.h>

#include "base/callback.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"

using MenuItemCallback = base::RepeatingCallback<void(NSMenuItem*)>;

namespace {

void UpdateItemForWebContents(NSMenuItem* item,
                              content::WebContents* web_contents) {
  TabUIHelper* tab_ui_helper = TabUIHelper::FromWebContents(web_contents);

  item.title = base::SysUTF16ToNSString(tab_ui_helper->GetTitle());
  item.image = tab_ui_helper->GetFavicon().AsNSImage();
}

}  // namespace

@interface TabMenuListener : NSObject
- (instancetype)initWithCallback:(MenuItemCallback)callback;
- (void)activateTab:(id)sender;
@end

@implementation TabMenuListener {
  MenuItemCallback callback_;
}

- (instancetype)initWithCallback:(MenuItemCallback)callback {
  if ((self = [super init])) {
    callback_ = callback;
  }
  return self;
}

- (IBAction)activateTab:(id)sender {
  callback_.Run(sender);
}
@end

TabMenuBridge::TabMenuBridge(TabStripModel* model, NSMenuItem* menu_item)
    : model_(model), menu_item_(menu_item) {
  menu_listener_.reset([[TabMenuListener alloc]
      initWithCallback:base::Bind(&TabMenuBridge::OnDynamicItemChosen,
                                  // Unretained is safe here: this class owns
                                  // MenuListener, which holds the callback
                                  // being constructed here, so the callback
                                  // will be destructed before this class.
                                  base::Unretained(this))]);
  model_->AddObserver(this);
}

TabMenuBridge::~TabMenuBridge() {
  if (model_)
    model_->RemoveObserver(this);
  RemoveAllDynamicItems();
}

void TabMenuBridge::BuildMenu() {
  DCHECK(model_);
  RemoveAllDynamicItems();
  AddDynamicItemsFromModel();
}

void TabMenuBridge::RemoveAllDynamicItems() {
  for (NSMenuItem* item in menu_item_.submenu.itemArray) {
    if (item.target == menu_listener_.get())
      [menu_item_.submenu removeItem:item];
  }
}

void TabMenuBridge::AddDynamicItemsFromModel() {
  dynamic_items_start_ = menu_item_.submenu.numberOfItems;
  for (int i = 0; i < model_->count(); ++i) {
    base::scoped_nsobject<NSMenuItem> item([[NSMenuItem alloc]
        initWithTitle:@""
               action:@selector(activateTab:)
        keyEquivalent:@""]);
    [item setTarget:menu_listener_.get()];
    UpdateItemForWebContents(item, model_->GetWebContentsAt(i));
    [menu_item_.submenu addItem:item.get()];
  }
}

void TabMenuBridge::OnDynamicItemChosen(NSMenuItem* item) {
  if (!model_)
    return;

  DCHECK_EQ(item.target, menu_listener_.get());
  int index = [menu_item_.submenu indexOfItem:item] - dynamic_items_start_;
  model_->ActivateTabAt(index, TabStripModel::UserGestureDetails(
                                   TabStripModel::GestureType::kTabMenu));
}

void TabMenuBridge::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  DCHECK(tab_strip_model);
  DCHECK_EQ(tab_strip_model, model_);

  // Rather than doing clever updating from |change|, just destroy the dynamic
  // menu items and re-add them.
  RemoveAllDynamicItems();
  AddDynamicItemsFromModel();
}

void TabMenuBridge::TabChangedAt(content::WebContents* contents,
                                 int index,
                                 TabChangeType change_type) {
  DCHECK(model_);

  // Ignore loading state changes - they happen very often during page load and
  // are used to drive the load spinner, which is not interesting to this menu.
  if (change_type == TabChangeType::kLoadingOnly)
    return;

  int menu_index = index + dynamic_items_start_;

  // It might seem like this can't happen but actually it can:
  // 1) Someone calls TabMenuModel::AddWebContents
  // 2) Some other observer (not this) is notified of the add
  // 3) That observer responds by doing something that eventually leads into
  //    UpdateWebContentsStateAt, while this class still hasn't observed the
  //    OnTabStripModelChanged (but the method that will notify us is on the
  //    stack)
  // 4) That UpdateWebContentsStateAt causes this object to observe a
  //    TabChangedAt for an index it hasn't yet been informed exists
  // As such, this code early-outs instead of DCHECKing. The newly-added
  // WebContents will be picked up later anyway when this object does get
  // notified of the addition.
  if (menu_index < 0 || menu_index >= menu_item_.submenu.numberOfItems)
    return;

  NSMenuItem* item = [menu_item_.submenu itemAtIndex:menu_index];
  UpdateItemForWebContents(item, contents);
}

void TabMenuBridge::OnTabStripModelDestroyed(TabStripModel* model) {
  model_->RemoveObserver(this);
  model_ = nullptr;
}
