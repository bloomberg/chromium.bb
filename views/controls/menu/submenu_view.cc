// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/menu/submenu_view.h"

#include "ui/gfx/canvas.h"
#include "views/controls/menu/menu_config.h"
#include "views/controls/menu/menu_controller.h"
#include "views/controls/menu/menu_host.h"
#include "views/controls/menu/menu_scroll_view_container.h"
#include "views/widget/root_view.h"

// Height of the drop indicator. This should be an even number.
static const int kDropIndicatorHeight = 2;

// Color of the drop indicator.
static const SkColor kDropIndicatorColor = SK_ColorBLACK;

namespace views {

// static
const int SubmenuView::kSubmenuBorderSize = 3;

// static
const char SubmenuView::kViewClassName[] = "views/SubmenuView";

SubmenuView::SubmenuView(MenuItemView* parent)
    : parent_menu_item_(parent),
      host_(NULL),
      drop_item_(NULL),
      drop_position_(MenuDelegate::DROP_NONE),
      scroll_view_container_(NULL),
      max_accelerator_width_(0) {
  DCHECK(parent);
  // We'll delete ourselves, otherwise the ScrollView would delete us on close.
  set_parent_owned(false);
}

SubmenuView::~SubmenuView() {
  // The menu may not have been closed yet (it will be hidden, but not
  // necessarily closed).
  Close();

  delete scroll_view_container_;
}

int SubmenuView::GetMenuItemCount() {
  int count = 0;
  for (int i = 0; i < GetChildViewCount(); ++i) {
    if (GetChildViewAt(i)->GetID() == MenuItemView::kMenuItemViewID)
      count++;
  }
  return count;
}

MenuItemView* SubmenuView::GetMenuItemAt(int index) {
  for (int i = 0, count = 0; i < GetChildViewCount(); ++i) {
    if (GetChildViewAt(i)->GetID() == MenuItemView::kMenuItemViewID &&
        count++ == index) {
      return static_cast<MenuItemView*>(GetChildViewAt(i));
    }
  }
  NOTREACHED();
  return NULL;
}

void SubmenuView::Layout() {
  // We're in a ScrollView, and need to set our width/height ourselves.
  View* parent = GetParent();
  if (!parent)
    return;

  // Use our current y, unless it means part of the menu isn't visible anymore.
  int pref_height = GetPreferredSize().height();
  int new_y;
  if (pref_height > parent->height())
    new_y = std::max(parent->height() - pref_height, y());
  else
    new_y = 0;
  SetBounds(x(), new_y, parent->width(), pref_height);

  gfx::Insets insets = GetInsets();
  int x = insets.left();
  int y = insets.top();
  int menu_item_width = width() - insets.width();
  for (int i = 0; i < GetChildViewCount(); ++i) {
    View* child = GetChildViewAt(i);
    if (child->IsVisible()) {
      gfx::Size child_pref_size = child->GetPreferredSize();
      child->SetBounds(x, y, menu_item_width, child_pref_size.height());
      y += child_pref_size.height();
    }
  }
}

gfx::Size SubmenuView::GetPreferredSize() {
  if (GetChildViewCount() == 0)
    return gfx::Size();

  max_accelerator_width_ = 0;
  int max_width = 0;
  int height = 0;
  for (int i = 0; i < GetChildViewCount(); ++i) {
    View* child = GetChildViewAt(i);
    gfx::Size child_pref_size = child->IsVisible() ?
        child->GetPreferredSize() : gfx::Size();
    max_width = std::max(max_width, child_pref_size.width());
    height += child_pref_size.height();
    if (child->GetID() == MenuItemView::kMenuItemViewID) {
      MenuItemView* menu = static_cast<MenuItemView*>(child);
      max_accelerator_width_ =
          std::max(max_accelerator_width_, menu->GetAcceleratorTextWidth());
    }
  }
  if (max_accelerator_width_ > 0) {
    max_accelerator_width_ +=
        MenuConfig::instance().label_to_accelerator_padding;
  }
  gfx::Insets insets = GetInsets();
  return gfx::Size(max_width + max_accelerator_width_ + insets.width(),
                   height + insets.height());
}

void SubmenuView::OnBoundsChanged() {
  SchedulePaint();
}

AccessibilityTypes::Role SubmenuView::GetAccessibleRole() {
  return AccessibilityTypes::ROLE_MENUPOPUP;
}

void SubmenuView::PaintChildren(gfx::Canvas* canvas) {
  View::PaintChildren(canvas);

  if (drop_item_ && drop_position_ != MenuDelegate::DROP_ON)
    PaintDropIndicator(canvas, drop_item_, drop_position_);
}

bool SubmenuView::GetDropFormats(
      int* formats,
      std::set<OSExchangeData::CustomFormat>* custom_formats) {
  DCHECK(GetMenuItem()->GetMenuController());
  return GetMenuItem()->GetMenuController()->GetDropFormats(this, formats,
                                                            custom_formats);
}

bool SubmenuView::AreDropTypesRequired() {
  DCHECK(GetMenuItem()->GetMenuController());
  return GetMenuItem()->GetMenuController()->AreDropTypesRequired(this);
}

bool SubmenuView::CanDrop(const OSExchangeData& data) {
  DCHECK(GetMenuItem()->GetMenuController());
  return GetMenuItem()->GetMenuController()->CanDrop(this, data);
}

void SubmenuView::OnDragEntered(const DropTargetEvent& event) {
  DCHECK(GetMenuItem()->GetMenuController());
  GetMenuItem()->GetMenuController()->OnDragEntered(this, event);
}

int SubmenuView::OnDragUpdated(const DropTargetEvent& event) {
  DCHECK(GetMenuItem()->GetMenuController());
  return GetMenuItem()->GetMenuController()->OnDragUpdated(this, event);
}

void SubmenuView::OnDragExited() {
  DCHECK(GetMenuItem()->GetMenuController());
  GetMenuItem()->GetMenuController()->OnDragExited(this);
}

int SubmenuView::OnPerformDrop(const DropTargetEvent& event) {
  DCHECK(GetMenuItem()->GetMenuController());
  return GetMenuItem()->GetMenuController()->OnPerformDrop(this, event);
}

bool SubmenuView::OnMouseWheel(const MouseWheelEvent& e) {
  gfx::Rect vis_bounds = GetVisibleBounds();
  int menu_item_count = GetMenuItemCount();
  if (vis_bounds.height() == height() || !menu_item_count) {
    // All menu items are visible, nothing to scroll.
    return true;
  }

  // Find the index of the first menu item whose y-coordinate is >= visible
  // y-coordinate.
  int i = 0;
  while ((i < menu_item_count) && (GetMenuItemAt(i)->y() < vis_bounds.y()))
    ++i;
  if (i == menu_item_count)
    return true;
  int first_vis_index = std::max(0,
      (GetMenuItemAt(i)->y() == vis_bounds.y()) ? i : i - 1);

  // If the first item isn't entirely visible, make it visible, otherwise make
  // the next/previous one entirely visible.
#if defined(OS_WIN)
  int delta = abs(e.GetOffset() / WHEEL_DELTA);
#elif defined(OS_LINUX)
  int delta = abs(e.GetOffset());
#endif
  for (bool scroll_up = (e.GetOffset() > 0); delta != 0; --delta) {
    int scroll_target;
    if (scroll_up) {
      if (GetMenuItemAt(first_vis_index)->y() == vis_bounds.y()) {
        if (first_vis_index == 0)
          break;
        first_vis_index--;
      }
      scroll_target = GetMenuItemAt(first_vis_index)->y();
    } else {
      if (first_vis_index + 1 == menu_item_count)
        break;
      scroll_target = GetMenuItemAt(first_vis_index + 1)->y();
      if (GetMenuItemAt(first_vis_index)->y() == vis_bounds.y())
        first_vis_index++;
    }
    ScrollRectToVisible(gfx::Rect(gfx::Point(0, scroll_target),
                                  vis_bounds.size()));
    vis_bounds = GetVisibleBounds();
  }

  return true;
}

bool SubmenuView::IsShowing() {
  return host_ && host_->IsMenuHostVisible();
}

void SubmenuView::ShowAt(gfx::NativeWindow parent,
                         const gfx::Rect& bounds,
                         bool do_capture) {
  if (host_) {
    host_->ShowMenuHost(do_capture);

    GetScrollViewContainer()->NotifyAccessibilityEvent(
        AccessibilityTypes::EVENT_MENUSTART);

    NotifyAccessibilityEvent(AccessibilityTypes::EVENT_MENUPOPUPSTART);
    return;
  }

  host_ = MenuHost::Create(this);
  // Force construction of the scroll view container.
  GetScrollViewContainer();
  // Make sure the first row is visible.
  ScrollRectToVisible(gfx::Rect(gfx::Point(), gfx::Size(1, 1)));
  host_->Init(parent, bounds, scroll_view_container_, do_capture);

  GetScrollViewContainer()->NotifyAccessibilityEvent(
      AccessibilityTypes::EVENT_MENUSTART);

  NotifyAccessibilityEvent(AccessibilityTypes::EVENT_MENUPOPUPSTART);
}

void SubmenuView::Reposition(const gfx::Rect& bounds) {
  if (host_)
    host_->SetMenuHostBounds(bounds);
}

void SubmenuView::Close() {
  if (host_) {
    NotifyAccessibilityEvent(AccessibilityTypes::EVENT_MENUPOPUPEND);

    GetScrollViewContainer()->NotifyAccessibilityEvent(
        AccessibilityTypes::EVENT_MENUEND);

    host_->DestroyMenuHost();
    host_ = NULL;
  }
}

void SubmenuView::Hide() {
  if (host_)
    host_->HideMenuHost();
}

void SubmenuView::ReleaseCapture() {
  if (host_)
    host_->ReleaseMenuHostCapture();
}

bool SubmenuView::SkipDefaultKeyEventProcessing(const views::KeyEvent& e) {
  return views::FocusManager::IsTabTraversalKeyEvent(e);
}

void SubmenuView::SetDropMenuItem(MenuItemView* item,
                                  MenuDelegate::DropPosition position) {
  if (drop_item_ == item && drop_position_ == position)
    return;
  SchedulePaintForDropIndicator(drop_item_, drop_position_);
  drop_item_ = item;
  drop_position_ = position;
  SchedulePaintForDropIndicator(drop_item_, drop_position_);
}

bool SubmenuView::GetShowSelection(MenuItemView* item) {
  if (drop_item_ == NULL)
    return true;
  // Something is being dropped on one of this menus items. Show the
  // selection if the drop is on the passed in item and the drop position is
  // ON.
  return (drop_item_ == item && drop_position_ == MenuDelegate::DROP_ON);
}

MenuScrollViewContainer* SubmenuView::GetScrollViewContainer() {
  if (!scroll_view_container_) {
    scroll_view_container_ = new MenuScrollViewContainer(this);
    // Otherwise MenuHost would delete us.
    scroll_view_container_->set_parent_owned(false);

    // Use the parent menu item accessible name for the menu view.
    string16 accessible_name;
    GetMenuItem()->GetAccessibleName(&accessible_name);
    scroll_view_container_->SetAccessibleName(accessible_name);
    SetAccessibleName(accessible_name);
  }
  return scroll_view_container_;
}

gfx::NativeWindow SubmenuView::native_window() const {
  return host_ ? host_->GetMenuHostWindow() : NULL;
}

void SubmenuView::MenuHostDestroyed() {
  host_ = NULL;
  GetMenuItem()->GetMenuController()->Cancel(MenuController::EXIT_DESTROYED);
}

std::string SubmenuView::GetClassName() const {
  return kViewClassName;
}

void SubmenuView::PaintDropIndicator(gfx::Canvas* canvas,
                                     MenuItemView* item,
                                     MenuDelegate::DropPosition position) {
  if (position == MenuDelegate::DROP_NONE)
    return;

  gfx::Rect bounds = CalculateDropIndicatorBounds(item, position);
  canvas->FillRectInt(kDropIndicatorColor, bounds.x(), bounds.y(),
                      bounds.width(), bounds.height());
}

void SubmenuView::SchedulePaintForDropIndicator(
    MenuItemView* item,
    MenuDelegate::DropPosition position) {
  if (item == NULL)
    return;

  if (position == MenuDelegate::DROP_ON) {
    item->SchedulePaint();
  } else if (position != MenuDelegate::DROP_NONE) {
    SchedulePaint(CalculateDropIndicatorBounds(item, position), false);
  }
}

gfx::Rect SubmenuView::CalculateDropIndicatorBounds(
    MenuItemView* item,
    MenuDelegate::DropPosition position) {
  DCHECK(position != MenuDelegate::DROP_NONE);
  gfx::Rect item_bounds = item->bounds();
  switch (position) {
    case MenuDelegate::DROP_BEFORE:
      item_bounds.Offset(0, -kDropIndicatorHeight / 2);
      item_bounds.set_height(kDropIndicatorHeight);
      return item_bounds;

    case MenuDelegate::DROP_AFTER:
      item_bounds.Offset(0, item_bounds.height() - kDropIndicatorHeight / 2);
      item_bounds.set_height(kDropIndicatorHeight);
      return item_bounds;

    default:
      // Don't render anything for on.
      return gfx::Rect();
  }
}

}  // namespace views
