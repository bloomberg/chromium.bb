// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_MENU_MENU_DELEGATE_H_
#define VIEWS_CONTROLS_MENU_MENU_DELEGATE_H_
#pragma once

#include <set>
#include <string>

#include "base/logging.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "views/controls/menu/menu_item_view.h"
#include "views/events/event.h"

using ui::OSExchangeData;

namespace views {

class DropTargetEvent;
class MenuButton;

// MenuDelegate --------------------------------------------------------------

// Delegate for a menu. This class is used as part of MenuItemView, see it
// for details.
// TODO(sky): merge this with ui::MenuModel.
class MenuDelegate {
 public:
  // Used during drag and drop to indicate where the drop indicator should
  // be rendered.
  enum DropPosition {
    // Indicates a drop is not allowed here.
    DROP_NONE,

    // Indicates the drop should occur before the item.
    DROP_BEFORE,

    // Indicates the drop should occur after the item.
    DROP_AFTER,

    // Indicates the drop should occur on the item.
    DROP_ON
  };

  // Whether or not an item should be shown as checked. This is invoked for
  // radio buttons and check buttons.
  virtual bool IsItemChecked(int id) const;

  // The string shown for the menu item. This is only invoked when an item is
  // added with an empty label.
  virtual std::wstring GetLabel(int id) const;

  // The tooltip shown for the menu item. This is invoked when the user
  // hovers over the item, and no tooltip text has been set for that item.
  virtual std::wstring GetTooltipText(int id, const gfx::Point& screen_loc);

  // If there is an accelerator for the menu item with id |id| it is set in
  // |accelerator| and true is returned.
  virtual bool GetAccelerator(int id, Accelerator* accelerator);

  // Shows the context menu with the specified id. This is invoked when the
  // user does the appropriate gesture to show a context menu. The id
  // identifies the id of the menu to show the context menu for.
  // is_mouse_gesture is true if this is the result of a mouse gesture.
  // If this is not the result of a mouse gesture |p| is the recommended
  // location to display the content menu at. In either case, |p| is in
  // screen coordinates.
  // Returns true if a context menu was displayed, otherwise false
  virtual bool ShowContextMenu(MenuItemView* source,
                               int id,
                               const gfx::Point& p,
                               bool is_mouse_gesture);

  // Controller
  virtual bool SupportsCommand(int id) const;
  virtual bool IsCommandEnabled(int id) const;
  virtual bool GetContextualLabel(int id, std::wstring* out) const;
  virtual void ExecuteCommand(int id) {
  }

  // If nested menus are showing (nested menus occur when a menu shows a context
  // menu) this is invoked to determine if all the menus should be closed when
  // the user selects the menu with the command |id|. This returns true to
  // indicate that all menus should be closed. Return false if only the
  // context menu should be closed.
  virtual bool ShouldCloseAllMenusOnExecute(int id);

  // Executes the specified command. mouse_event_flags give the flags of the
  // mouse event that triggered this to be invoked (views::MouseEvent
  // flags). mouse_event_flags is 0 if this is triggered by a user gesture
  // other than a mouse event.
  virtual void ExecuteCommand(int id, int mouse_event_flags);

  // Returns true if the specified mouse event is one the user can use
  // to trigger, or accept, the mouse. Defaults to left or right mouse buttons.
  virtual bool IsTriggerableEvent(const MouseEvent& e);

  // Invoked to determine if drops can be accepted for a submenu. This is
  // ONLY invoked for menus that have submenus and indicates whether or not
  // a drop can occur on any of the child items of the item. For example,
  // consider the following menu structure:
  //
  // A
  //   B
  //   C
  //
  // Where A has a submenu with children B and C. This is ONLY invoked for
  // A, not B and C.
  //

  // To restrict which children can be dropped on override GetDropOperation.
  virtual bool CanDrop(MenuItemView* menu, const OSExchangeData& data);

  // See view for a description of this method.
  virtual bool GetDropFormats(
      MenuItemView* menu,
      int* formats,
      std::set<OSExchangeData::CustomFormat>* custom_formats);

  // See view for a description of this method.
  virtual bool AreDropTypesRequired(MenuItemView* menu);

  // Returns the drop operation for the specified target menu item. This is
  // only invoked if CanDrop returned true for the parent menu. position
  // is set based on the location of the mouse, reset to specify a different
  // position.
  //
  // If a drop should not be allowed, returned ui::DragDropTypes::DRAG_NONE.
  virtual int GetDropOperation(MenuItemView* item,
                               const DropTargetEvent& event,
                               DropPosition* position);

  // Invoked to perform the drop operation. This is ONLY invoked if
  // canDrop returned true for the parent menu item, and GetDropOperation
  // returned an operation other than ui::DragDropTypes::DRAG_NONE.
  //
  // menu indicates the menu the drop occurred on.
  virtual int OnPerformDrop(MenuItemView* menu,
                            DropPosition position,
                            const DropTargetEvent& event);

  // Invoked to determine if it is possible for the user to drag the specified
  // menu item.
  virtual bool CanDrag(MenuItemView* menu);

  // Invoked to write the data for a drag operation to data. sender is the
  // MenuItemView being dragged.
  virtual void WriteDragData(MenuItemView* sender, OSExchangeData* data);

  // Invoked to determine the drag operations for a drag session of sender.
  // See DragDropTypes for possible values.
  virtual int GetDragOperations(MenuItemView* sender);

  // Notification the menu has closed. This is only sent when running the
  // menu for a drop.
  virtual void DropMenuClosed(MenuItemView* menu) {
  }

  // Notification that the user has highlighted the specified item.
  virtual void SelectionChanged(MenuItemView* menu) {
  }

  // If the user drags the mouse outside the bounds of the menu the delegate
  // is queried for a sibling menu to show. If this returns non-null the
  // current menu is hidden, and the menu returned from this method is shown.
  //
  // The delegate owns the returned menu, not the controller.
  virtual MenuItemView* GetSiblingMenu(MenuItemView* menu,
                                       const gfx::Point& screen_point,
                                       MenuItemView::AnchorPosition* anchor,
                                       bool* has_mnemonics,
                                       MenuButton** button);

  // Returns the max width menus can grow to be.
  virtual int GetMaxWidthForMenu();
};

}  // namespace views

#endif  // VIEWS_CONTROLS_MENU_MENU_DELEGATE_H_
