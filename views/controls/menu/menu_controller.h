// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_MENU_MENU_CONTROLLER_H_
#define VIEWS_CONTROLS_MENU_MENU_CONTROLLER_H_

#include "build/build_config.h"

#include <list>
#include <set>
#include <vector>

#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "base/timer.h"
#include "views/controls/menu/menu_delegate.h"
#include "views/controls/menu/menu_item_view.h"

class OSExchangeData;

namespace views {

class DropTargetEvent;
class MenuButton;
class MenuHostRootView;
class MouseEvent;
class SubmenuView;
class View;

// MenuController -------------------------------------------------------------

// MenuController is used internally by the various menu classes to manage
// showing, selecting and drag/drop for menus. All relevant events are
// forwarded to the MenuController from SubmenuView and MenuHost.
class MenuController : public MessageLoopForUI::Dispatcher {
 public:
  friend class MenuHostRootView;
  friend class MenuItemView;

  // If a menu is currently active, this returns the controller for it.
  static MenuController* GetActiveInstance();

  // Runs the menu at the specified location. If the menu was configured to
  // block, the selected item is returned. If the menu does not block this
  // returns NULL immediately.
  MenuItemView* Run(gfx::NativeWindow parent,
                    MenuButton* button,
                    MenuItemView* root,
                    const gfx::Rect& bounds,
                    MenuItemView::AnchorPosition position,
                    int* mouse_event_flags);

  // Whether or not Run blocks.
  bool IsBlockingRun() const { return blocking_run_; }

  // Sets the selection to menu_item, a value of NULL unselects everything.
  // If open_submenu is true and menu_item has a submenu, the submenu is shown.
  // If update_immediately is true, submenus are opened immediately, otherwise
  // submenus are only opened after a timer fires.
  //
  // Internally this updates pending_state_ immediatley, and if
  // update_immediately is true, CommitPendingSelection is invoked to
  // show/hide submenus and update state_.
  void SetSelection(MenuItemView* menu_item,
                    bool open_submenu,
                    bool update_immediately);

  // Cancels the current Run. If all is true, any nested loops are canceled
  // as well. This immediately hides all menus.
  void Cancel(bool all);

  // An alternative to Cancel(true) that can be used with a OneShotTimer.
  void CancelAll() { return Cancel(true); }

  // Various events, forwarded from the submenu.
  //
  // NOTE: the coordinates of the events are in that of the
  // MenuScrollViewContainer.
  void OnMousePressed(SubmenuView* source, const MouseEvent& event);
  void OnMouseDragged(SubmenuView* source, const MouseEvent& event);
  void OnMouseReleased(SubmenuView* source, const MouseEvent& event);
  void OnMouseMoved(SubmenuView* source, const MouseEvent& event);
  void OnMouseEntered(SubmenuView* source, const MouseEvent& event);
  bool GetDropFormats(
      SubmenuView* source,
      int* formats,
      std::set<OSExchangeData::CustomFormat>* custom_formats);
  bool AreDropTypesRequired(SubmenuView* source);
  bool CanDrop(SubmenuView* source, const OSExchangeData& data);
  void OnDragEntered(SubmenuView* source, const DropTargetEvent& event);
  int OnDragUpdated(SubmenuView* source, const DropTargetEvent& event);
  void OnDragExited(SubmenuView* source);
  int OnPerformDrop(SubmenuView* source, const DropTargetEvent& event);

  // Invoked from the scroll buttons of the MenuScrollViewContainer.
  void OnDragEnteredScrollButton(SubmenuView* source, bool is_up);
  void OnDragExitedScrollButton(SubmenuView* source);

 private:
  // Enumeration of how the menu should exit.
  enum ExitType {
    // Don't exit.
    EXIT_NONE,

    // All menus, including nested, should be exited.
    EXIT_ALL,

    // Only the outermost menu should be exited.
    EXIT_OUTERMOST
  };

  class MenuScrollTask;

  // Tracks selection information.
  struct State {
    State() : item(NULL), submenu_open(false) {}

    // The selected menu item.
    MenuItemView* item;

    // If item has a submenu this indicates if the submenu is showing.
    bool submenu_open;

    // Bounds passed to the run menu. Used for positioning the first menu.
    gfx::Rect initial_bounds;

    // Position of the initial menu.
    MenuItemView::AnchorPosition anchor;

    // The direction child menus have opened in.
    std::list<bool> open_leading;

    // Bounds for the monitor we're showing on.
    gfx::Rect monitor_bounds;
  };

  // Used by GetMenuPartByScreenCoordinate to indicate the menu part at a
  // particular location.
  struct MenuPart {
    // Type of part.
    enum Type {
      NONE,
      MENU_ITEM,
      SCROLL_UP,
      SCROLL_DOWN
    };

    MenuPart() : type(NONE), menu(NULL), parent(NULL), submenu(NULL) {}

    // Convenience for testing type == SCROLL_DOWN or type == SCROLL_UP.
    bool is_scroll() const { return type == SCROLL_DOWN || type == SCROLL_UP; }

    // Type of part.
    Type type;

    // If type is MENU_ITEM, this is the menu item the mouse is over, otherwise
    // this is NULL.
    // NOTE: if type is MENU_ITEM and the mouse is not over a valid menu item
    //       but is over a menu (for example, the mouse is over a separator or
    //       empty menu), this is NULL and parent is the menu the mouse was
    //       clicked on.
    MenuItemView* menu;

    // If type is MENU_ITEM but the mouse is not over a menu item this is the
    // parent of the menu item the user clicked on. Otherwise this is NULL.
    MenuItemView* parent;

    // If type is SCROLL_*, this is the submenu the mouse is over.
    SubmenuView* submenu;
  };

  // Sets the active MenuController.
  static void SetActiveInstance(MenuController* controller);

#if defined(OS_WIN)
  // Dispatcher method. This returns true if the menu was canceled, or
  // if the message is such that the menu should be closed.
  virtual bool Dispatch(const MSG& msg);

#else
  virtual bool Dispatch(GdkEvent* event);
#endif

  // Key processing. The return value of this is returned from Dispatch.
  // In other words, if this returns false (which happens if escape was
  // pressed, or a matching mnemonic was found) the message loop returns.
#if defined(OS_WIN)
  bool OnKeyDown(int key_code, const MSG& msg);
#else
  bool OnKeyDown(int key_code);
#endif

  // Creates a MenuController. If blocking is true, Run blocks the caller
  explicit MenuController(bool blocking);

  ~MenuController();

  void UpdateInitialLocation(const gfx::Rect& bounds,
                             MenuItemView::AnchorPosition position);

  // Invoked when the user accepts the selected item. This is only used
  // when blocking. This schedules the loop to quit.
  void Accept(MenuItemView* item, int mouse_event_flags);

  bool ShowSiblingMenu(SubmenuView* source, const MouseEvent& e);

  // Closes all menus, including any menus of nested invocations of Run.
  void CloseAllNestedMenus();

  // Gets the enabled menu item at the specified location.
  // If over_any_menu is non-null it is set to indicate whether the location
  // is over any menu. It is possible for this to return NULL, but
  // over_any_menu to be true. For example, the user clicked on a separator.
  MenuItemView* GetMenuItemAt(View* menu, int x, int y);

  // If there is an empty menu item at the specified location, it is returned.
  MenuItemView* GetEmptyMenuItemAt(View* source, int x, int y);

  // Returns true if the coordinate is over the scroll buttons of the
  // SubmenuView's MenuScrollViewContainer. If true is returned, part is set to
  // indicate which scroll button the coordinate is.
  bool IsScrollButtonAt(SubmenuView* source,
                        int x,
                        int y,
                        MenuPart::Type* part);

  // Returns the target for the mouse event.
  MenuPart GetMenuPartByScreenCoordinate(SubmenuView* source,
                                         int source_x,
                                         int source_y);

  // Implementation of GetMenuPartByScreenCoordinate for a single menu. Returns
  // true if the supplied SubmenuView contains the location in terms of the
  // screen. If it does, part is set appropriately and true is returned.
  bool GetMenuPartByScreenCoordinateImpl(SubmenuView* menu,
                                         const gfx::Point& screen_loc,
                                         MenuPart* part);

  // Returns true if the SubmenuView contains the specified location. This does
  // NOT included the scroll buttons, only the submenu view.
  bool DoesSubmenuContainLocation(SubmenuView* submenu,
                                  const gfx::Point& screen_loc);

  // Opens/Closes the necessary menus such that state_ matches that of
  // pending_state_. This is invoked if submenus are not opened immediately,
  // but after a delay.
  void CommitPendingSelection();

  // If item has a submenu, it is closed. This does NOT update the selection
  // in anyway.
  void CloseMenu(MenuItemView* item);

  // If item has a submenu, it is opened. This does NOT update the selection
  // in anyway.
  void OpenMenu(MenuItemView* item);

  // Implementation of OpenMenu. If |show| is true, this invokes show on the
  // menu, otherwise Reposition is invoked.
  void OpenMenuImpl(MenuItemView* item, bool show);

  // Invoked when the children of a menu change and the menu is showing.
  // This closes any submenus and resizes the submenu.
  void MenuChildrenChanged(MenuItemView* item);

  // Builds the paths of the two menu items into the two paths, and
  // sets first_diff_at to the location of the first difference between the
  // two paths.
  void BuildPathsAndCalculateDiff(MenuItemView* old_item,
                                  MenuItemView* new_item,
                                  std::vector<MenuItemView*>* old_path,
                                  std::vector<MenuItemView*>* new_path,
                                  size_t* first_diff_at);

  // Builds the path for the specified item.
  void BuildMenuItemPath(MenuItemView* item, std::vector<MenuItemView*>* path);

  // Starts/stops the timer that commits the pending state to state
  // (opens/closes submenus).
  void StartShowTimer();
  void StopShowTimer();

  // Starts/stops the timer cancel the menu. This is used during drag and
  // drop when the drop enters/exits the menu.
  void StartCancelAllTimer();
  void StopCancelAllTimer();

  // Calculates the bounds of the menu to show. is_leading is set to match the
  // direction the menu opened in.
  gfx::Rect CalculateMenuBounds(MenuItemView* item,
                                bool prefer_leading,
                                bool* is_leading);

  // Returns the depth of the menu.
  static int MenuDepth(MenuItemView* item);

  // Selects the next/previous menu item.
  void IncrementSelection(int delta);

  // If the selected item has a submenu and it isn't currently open, the
  // the selection is changed such that the menu opens immediately.
  void OpenSubmenuChangeSelectionIfCan();

  // If possible, closes the submenu.
  void CloseSubmenu();

  // Selects by mnemonic, and if that doesn't work tries the first character of
  // the title. Returns true if a match was selected and the menu should exit.
  bool SelectByChar(wchar_t key);

#if defined(OS_WIN)
  // If there is a window at the location of the event, a new mouse event is
  // generated and posted to it.
  void RepostEvent(SubmenuView* source, const MouseEvent& event);
#endif

  // Sets the drop target to new_item.
  void SetDropMenuItem(MenuItemView* new_item,
                       MenuDelegate::DropPosition position);

  // Starts/stops scrolling as appropriate. part gives the part the mouse is
  // over.
  void UpdateScrolling(const MenuPart& part);

  // Stops scrolling.
  void StopScrolling();

  // The active instance.
  static MenuController* active_instance_;

  // If true, Run blocks. If false, Run doesn't block and this is used for
  // drag and drop. Note that the semantics for drag and drop are slightly
  // different: cancel timer is kicked off any time the drag moves outside the
  // menu, mouse events do nothing...
  bool blocking_run_;

  // If true, we're showing.
  bool showing_;

  // Indicates what to exit.
  ExitType exit_type_;

  // Whether we did a capture. We do a capture only if we're blocking and
  // the mouse was down when Run.
  bool did_capture_;

  // As the user drags the mouse around pending_state_ changes immediately.
  // When the user stops moving/dragging the mouse (or clicks the mouse)
  // pending_state_ is committed to state_, potentially resulting in
  // opening or closing submenus. This gives a slight delayed effect to
  // submenus as the user moves the mouse around. This is done so that as the
  // user moves the mouse all submenus don't immediately pop.
  State pending_state_;
  State state_;

  // If the user accepted the selection, this is the result.
  MenuItemView* result_;

  // The mouse event flags when the user clicked on a menu. Is 0 if the
  // user did not use the mouse to select the menu.
  int result_mouse_event_flags_;

  // If not empty, it means we're nested. When Run is invoked from within
  // Run, the current state (state_) is pushed onto menu_stack_. This allows
  // MenuController to restore the state when the nested run returns.
  std::list<State> menu_stack_;

  // As the mouse moves around submenus are not opened immediately. Instead
  // they open after this timer fires.
  base::OneShotTimer<MenuController> show_timer_;

  // Used to invoke CancelAll(). This is used during drag and drop to hide the
  // menu after the mouse moves out of the of the menu. This is necessitated by
  // the lack of an ability to detect when the drag has completed from the drop
  // side.
  base::OneShotTimer<MenuController> cancel_all_timer_;

  // Drop target.
  MenuItemView* drop_target_;
  MenuDelegate::DropPosition drop_position_;

  // Owner of child windows.
  gfx::NativeWindow owner_;

  // Indicates a possible drag operation.
  bool possible_drag_;

  // Location the mouse was pressed at. Used to detect d&d.
  int press_x_;
  int press_y_;

  // We get a slew of drag updated messages as the mouse is over us. To avoid
  // continually processing whether we can drop, we cache the coordinates.
  bool valid_drop_coordinates_;
  int drop_x_;
  int drop_y_;
  int last_drop_operation_;

  // If true, we're in the middle of invoking ShowAt on a submenu.
  bool showing_submenu_;

  // Task for scrolling the menu. If non-null indicates a scroll is currently
  // underway.
  scoped_ptr<MenuScrollTask> scroll_task_;

  MenuButton* menu_button_;

  DISALLOW_COPY_AND_ASSIGN(MenuController);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_MENU_MENU_CONTROLLER_H_
