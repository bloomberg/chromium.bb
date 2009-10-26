// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/menu/menu_item_view.h"

#include "app/gfx/canvas.h"
#include "app/l10n_util.h"
#include "grit/app_strings.h"
#include "views/controls/menu/menu_config.h"
#include "views/controls/menu/menu_controller.h"
#include "views/controls/menu/menu_separator.h"
#include "views/controls/menu/submenu_view.h"

namespace views {

namespace {

// EmptyMenuMenuItem ---------------------------------------------------------

// EmptyMenuMenuItem is used when a menu has no menu items. EmptyMenuMenuItem
// is itself a MenuItemView, but it uses a different ID so that it isn't
// identified as a MenuItemView.

class EmptyMenuMenuItem : public MenuItemView {
 public:
  explicit EmptyMenuMenuItem(MenuItemView* parent) :
      MenuItemView(parent, 0, NORMAL) {
    SetTitle(l10n_util::GetString(IDS_APP_MENU_EMPTY_SUBMENU));
    // Set this so that we're not identified as a normal menu item.
    SetID(kEmptyMenuItemViewID);
    SetEnabled(false);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(EmptyMenuMenuItem);
};

}  // namespace

// MenuItemView ---------------------------------------------------------------

//  static
const int MenuItemView::kMenuItemViewID = 1001;

// static
const int MenuItemView::kEmptyMenuItemViewID =
    MenuItemView::kMenuItemViewID + 1;

// static
bool MenuItemView::allow_task_nesting_during_run_ = false;

// static
int MenuItemView::label_start_;

// static
int MenuItemView::item_right_margin_;

// static
int MenuItemView::pref_menu_height_;

MenuItemView::MenuItemView(MenuDelegate* delegate) {
  // NOTE: don't check the delegate for NULL, UpdateMenuPartSizes supplies a
  // NULL delegate.
  Init(NULL, 0, SUBMENU, delegate);
}

MenuItemView::~MenuItemView() {
  // TODO(sky): ownership is bit wrong now. In particular if a nested message
  // loop is running deletion can't be done, otherwise the stack gets
  // thoroughly screwed. The destructor should be made private, and
  // MenuController should be the only place handling deletion of the menu.
  delete submenu_;
}

void MenuItemView::RunMenuAt(gfx::NativeWindow parent,
                             MenuButton* button,
                             const gfx::Rect& bounds,
                             AnchorPosition anchor,
                             bool has_mnemonics) {
  PrepareForRun(has_mnemonics);

  int mouse_event_flags;

  MenuController* controller = MenuController::GetActiveInstance();
  if (controller && !controller->IsBlockingRun()) {
    // A menu is already showing, but it isn't a blocking menu. Cancel it.
    // We can get here during drag and drop if the user right clicks on the
    // menu quickly after the drop.
    controller->Cancel(true);
    controller = NULL;
  }
  bool owns_controller = false;
  if (!controller) {
    // No menus are showing, show one.
    controller = new MenuController(true);
    MenuController::SetActiveInstance(controller);
    owns_controller = true;
  } else {
    // A menu is already showing, use the same controller.

    // Don't support blocking from within non-blocking.
    DCHECK(controller->IsBlockingRun());
  }

  controller_ = controller;

  // Run the loop.
  MenuItemView* result =
      controller->Run(parent, button, this, bounds, anchor, &mouse_event_flags);

  RemoveEmptyMenus();

  controller_ = NULL;

  if (owns_controller) {
    // We created the controller and need to delete it.
    if (MenuController::GetActiveInstance() == controller)
      MenuController::SetActiveInstance(NULL);
    delete controller;
  }
  // Make sure all the windows we created to show the menus have been
  // destroyed.
  DestroyAllMenuHosts();
  if (result && delegate_)
    delegate_->ExecuteCommand(result->GetCommand(), mouse_event_flags);
}

void MenuItemView::RunMenuForDropAt(gfx::NativeWindow parent,
                                    const gfx::Rect& bounds,
                                    AnchorPosition anchor) {
  PrepareForRun(false);

  // If there is a menu, hide it so that only one menu is shown during dnd.
  MenuController* current_controller = MenuController::GetActiveInstance();
  if (current_controller) {
    current_controller->Cancel(true);
  }

  // Always create a new controller for non-blocking.
  controller_ = new MenuController(false);

  // Set the instance, that way it can be canceled by another menu.
  MenuController::SetActiveInstance(controller_);

  controller_->Run(parent, NULL, this, bounds, anchor, NULL);
}

void MenuItemView::Cancel() {
  if (controller_ && !canceled_) {
    canceled_ = true;
    controller_->Cancel(true);
  }
}

SubmenuView* MenuItemView::CreateSubmenu() {
  if (!submenu_)
    submenu_ = new SubmenuView(this);
  return submenu_;
}

void MenuItemView::SetSelected(bool selected) {
  selected_ = selected;
  SchedulePaint();
}

void MenuItemView::SetIcon(const SkBitmap& icon, int item_id) {
  MenuItemView* item = GetDescendantByID(item_id);
  DCHECK(item);
  item->SetIcon(icon);
}

void MenuItemView::SetIcon(const SkBitmap& icon) {
  icon_ = icon;
  SchedulePaint();
}

void MenuItemView::Paint(gfx::Canvas* canvas) {
  Paint(canvas, false);
}

MenuController* MenuItemView::GetMenuController() {
  return GetRootMenuItem()->controller_;
}

MenuDelegate* MenuItemView::GetDelegate() {
  return GetRootMenuItem()->delegate_;
}

MenuItemView* MenuItemView::GetRootMenuItem() {
  MenuItemView* item = this;
  while (item) {
    MenuItemView* parent = item->GetParentMenuItem();
    if (!parent)
      return item;
    item = parent;
  }
  NOTREACHED();
  return NULL;
}

wchar_t MenuItemView::GetMnemonic() {
  if (!has_mnemonics_)
    return 0;

  const std::wstring& title = GetTitle();
  size_t index = 0;
  do {
    index = title.find('&', index);
    if (index != std::wstring::npos) {
      if (index + 1 != title.size() && title[index + 1] != '&')
        return title[index + 1];
      index++;
    }
  } while (index != std::wstring::npos);
  return 0;
}

MenuItemView::MenuItemView(MenuItemView* parent,
                           int command,
                           MenuItemView::Type type) {
  Init(parent, command, type, NULL);
}

// Calculates all sizes that we can from the OS.
//
// This is invoked prior to Running a menu.
void MenuItemView::UpdateMenuPartSizes(bool has_icons) {
  MenuConfig::Reset();
  const MenuConfig& config = MenuConfig::instance();

  item_right_margin_ = config.label_to_arrow_padding + config.arrow_width +
      config.arrow_to_edge_padding;

  if (has_icons) {
    label_start_ = config.item_left_margin + config.check_width +
                   config.icon_to_label_padding;
  } else {
    // If there are no icons don't pad by the icon to label padding. This
    // makes us look close to system menus.
    label_start_ = config.item_left_margin + config.check_width;
  }
  if (config.render_gutter)
    label_start_ += config.gutter_width + config.gutter_to_label;

  MenuItemView menu_item(NULL);
  menu_item.SetTitle(L"blah");  // Text doesn't matter here.
  pref_menu_height_ = menu_item.GetPreferredSize().height();
}

void MenuItemView::Init(MenuItemView* parent,
                        int command,
                        MenuItemView::Type type,
                        MenuDelegate* delegate) {
  delegate_ = delegate;
  controller_ = NULL;
  canceled_ = false;
  parent_menu_item_ = parent;
  type_ = type;
  selected_ = false;
  command_ = command;
  submenu_ = NULL;
  // Assign our ID, this allows SubmenuItemView to find MenuItemViews.
  SetID(kMenuItemViewID);
  has_icons_ = false;

  MenuDelegate* root_delegate = GetDelegate();
  if (root_delegate)
    SetEnabled(root_delegate->IsCommandEnabled(command));
}

MenuItemView* MenuItemView::AppendMenuItemInternal(int item_id,
                                                   const std::wstring& label,
                                                   const SkBitmap& icon,
                                                   Type type) {
  if (!submenu_)
    CreateSubmenu();
  if (type == SEPARATOR) {
    submenu_->AddChildView(new MenuSeparator());
    return NULL;
  }
  MenuItemView* item = new MenuItemView(this, item_id, type);
  if (label.empty() && GetDelegate())
    item->SetTitle(GetDelegate()->GetLabel(item_id));
  else
    item->SetTitle(label);
  item->SetIcon(icon);
  if (type == SUBMENU)
    item->CreateSubmenu();
  submenu_->AddChildView(item);
  return item;
}

MenuItemView* MenuItemView::GetDescendantByID(int id) {
  if (GetCommand() == id)
    return this;
  if (!HasSubmenu())
    return NULL;
  for (int i = 0; i < GetSubmenu()->GetChildViewCount(); ++i) {
    View* child = GetSubmenu()->GetChildViewAt(i);
    if (child->GetID() == MenuItemView::kMenuItemViewID) {
      MenuItemView* result = static_cast<MenuItemView*>(child)->
          GetDescendantByID(id);
      if (result)
        return result;
    }
  }
  return NULL;
}

void MenuItemView::DropMenuClosed(bool notify_delegate) {
  DCHECK(controller_);
  DCHECK(!controller_->IsBlockingRun());
  if (MenuController::GetActiveInstance() == controller_)
    MenuController::SetActiveInstance(NULL);
  delete controller_;
  controller_ = NULL;

  RemoveEmptyMenus();

  if (notify_delegate && delegate_) {
    // Our delegate is null when invoked from the destructor.
    delegate_->DropMenuClosed(this);
  }
  // WARNING: its possible the delegate deleted us at this point.
}

void MenuItemView::PrepareForRun(bool has_mnemonics) {
  // Currently we only support showing the root.
  DCHECK(!parent_menu_item_);

  // Force us to have a submenu.
  CreateSubmenu();

  canceled_ = false;

  has_mnemonics_ = has_mnemonics;

  AddEmptyMenus();

  if (!MenuController::GetActiveInstance()) {
    // Only update the menu size if there are no menus showing, otherwise
    // things may shift around.
    UpdateMenuPartSizes(has_icons_);
  }
}

int MenuItemView::GetDrawStringFlags() {
  int flags = 0;
  if (UILayoutIsRightToLeft())
    flags |= gfx::Canvas::TEXT_ALIGN_RIGHT;
  else
    flags |= gfx::Canvas::TEXT_ALIGN_LEFT;

  if (has_mnemonics_) {
    if (MenuConfig::instance().show_mnemonics)
      flags |= gfx::Canvas::SHOW_PREFIX;
    else
      flags |= gfx::Canvas::HIDE_PREFIX;
  }
  return flags;
}

void MenuItemView::AddEmptyMenus() {
  DCHECK(HasSubmenu());
  if (submenu_->GetChildViewCount() == 0) {
    submenu_->AddChildView(0, new EmptyMenuMenuItem(this));
  } else {
    for (int i = 0, item_count = submenu_->GetMenuItemCount(); i < item_count;
         ++i) {
      MenuItemView* child = submenu_->GetMenuItemAt(i);
      if (child->HasSubmenu())
        child->AddEmptyMenus();
    }
  }
}

void MenuItemView::RemoveEmptyMenus() {
  DCHECK(HasSubmenu());
  // Iterate backwards as we may end up removing views, which alters the child
  // view count.
  for (int i = submenu_->GetChildViewCount() - 1; i >= 0; --i) {
    View* child = submenu_->GetChildViewAt(i);
    if (child->GetID() == MenuItemView::kMenuItemViewID) {
      MenuItemView* menu_item = static_cast<MenuItemView*>(child);
      if (menu_item->HasSubmenu())
        menu_item->RemoveEmptyMenus();
    } else if (child->GetID() == EmptyMenuMenuItem::kEmptyMenuItemViewID) {
      submenu_->RemoveChildView(child);
    }
  }
}

void MenuItemView::AdjustBoundsForRTLUI(gfx::Rect* rect) const {
  rect->set_x(MirroredLeftPointForRect(*rect));
}


void MenuItemView::DestroyAllMenuHosts() {
  if (!HasSubmenu())
    return;

  submenu_->Close();
  for (int i = 0, item_count = submenu_->GetMenuItemCount(); i < item_count;
       ++i) {
    submenu_->GetMenuItemAt(i)->DestroyAllMenuHosts();
  }
}

int MenuItemView::GetTopMargin() {
  MenuItemView* root = GetRootMenuItem();
  return root && root->has_icons_
      ? MenuConfig::instance().item_top_margin :
        MenuConfig::instance().item_no_icon_top_margin;
}

int MenuItemView::GetBottomMargin() {
  MenuItemView* root = GetRootMenuItem();
  return root && root->has_icons_
      ? MenuConfig::instance().item_bottom_margin :
        MenuConfig::instance().item_no_icon_bottom_margin;
}

}  // namespace views
