// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef VIEWS_CONTROLS_MENU_NATIVE_MENU_GTK_H_
#define VIEWS_CONTROLS_MENU_NATIVE_MENU_GTK_H_

#include <gtk/gtk.h>

#include "base/task.h"
#include "views/controls/menu/menu_wrapper.h"

namespace menus {
class MenuModel;
}

namespace views {

// A Gtk implementation of MenuWrapper.
//
// NOTE: On windows the activate message is not sent immediately when an item
// is selected. Instead a message is added to the queue that is processed later.
// To simulate that (and avoid having to deal with different behavior between
// the platforms) we mimick that by posting a task after a menu item is selected
// then notify.
//
// TODO(beng): rename to MenuGtk once the old class is dead.
class NativeMenuGtk : public MenuWrapper {
 public:
  explicit NativeMenuGtk(Menu2* menu);
  virtual ~NativeMenuGtk();

  // Overridden from MenuWrapper:
  virtual void RunMenuAt(const gfx::Point& point, int alignment);
  virtual void CancelMenu();
  virtual void Rebuild();
  virtual void UpdateStates();
  virtual gfx::NativeMenu GetNativeMenu() const;

 private:
  static void OnMenuHidden(GtkWidget* widget, NativeMenuGtk* menu);

  void AddSeparatorAt(int index);
  GtkWidget* AddMenuItemAt(int index, GtkRadioMenuItem* radio_group);

  void ResetMenu();

  // Updates the menu item's state.
  void UpdateMenuItemState(GtkWidget* menu_item);

  static void UpdateStateCallback(GtkWidget* menu_item, gpointer data);

  // Callback for gtk_menu_popup to position the menu.
  static void MenuPositionFunc(GtkMenu* menu, int* x, int* y, gboolean* push_in,
                               void* data);

  // Event handlers:
  void OnActivate(GtkMenuItem* menu_item);

  // Gtk signal handlers.
  static void CallActivate(GtkMenuItem* menu_item, NativeMenuGtk* native_menu);

  // Sets the parent of this menu.
  void set_parent(NativeMenuGtk* parent) { parent_ = parent; }

  // Returns the root of the menu tree.
  NativeMenuGtk* GetAncestor();

  // Callback that we should really process the menu activation.
  // See description above class for why we delay processing activation.
  void ProcessActivate();

  // Notifies the model the user selected an item.
  void Activate();

  // A callback to delete menu2 object when the native widget is
  // destroyed first.
  static void MenuDestroyed(GtkWidget* widget, Menu2* menu2);

  // If we're a submenu, this is the parent.
  NativeMenuGtk* parent_;

  menus::MenuModel* model_;

  GtkWidget* menu_;

  bool menu_shown_;

  // A flag used to avoid misfiring ActivateAt call on the menu model.
  // This is necessary as GTK menu fires an activate signal even when the
  // state is changed by |UpdateStates()| API.
  bool suppress_activate_signal_;

  // If the user selects something from the menu this is the menu they selected
  // it from. When an item is selected menu_activated_ on the root ancestor is
  // set to the menu the user selected and after the nested message loop exits
  // Activate is invoked on this menu.
  NativeMenuGtk* activated_menu_;

  // The index of the item the user selected. This is set on the actual menu the
  // user selects and not the root.
  int activated_index_;

  // Used when a menu item is selected. See description above class as to why
  // we do this.
  ScopedRunnableMethodFactory<NativeMenuGtk> activate_factory_;

  // A eference to the hosting menu2 object and signal handler id
  // used to delete the menu2 when its native menu gtk is destroyed first.
  Menu2* host_menu_;
  gulong destroy_handler_id_;

  DISALLOW_COPY_AND_ASSIGN(NativeMenuGtk);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_MENU_NATIVE_MENU_GTK_H_
