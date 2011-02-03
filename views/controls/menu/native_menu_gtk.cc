// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/menu/native_menu_gtk.h"

#include <algorithm>
#include <map>
#include <string>

#include "base/i18n/rtl.h"
#include "base/message_loop.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "gfx/font.h"
#include "gfx/gtk_util.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/base/keycodes/keyboard_code_conversion_gtk.h"
#include "ui/base/models/menu_model.h"
#include "views/accelerator.h"
#include "views/controls/menu/menu_2.h"
#include "views/controls/menu/nested_dispatcher_gtk.h"

#if defined(TOUCH_UI)
#include "views/focus/accelerator_handler.h"
#endif

namespace {

const char kPositionString[] = "position";
const char kAccelGroupString[] = "accel_group";

// Key for the property set on the gtk menu that gives the handle to the hosting
// NativeMenuGtk.
const char kNativeMenuGtkString[] = "native_menu_gtk";

// Data passed to the MenuPositionFunc from gtk_menu_popup
struct Position {
  // The point to run the menu at.
  gfx::Point point;
  // The alignment of the menu at that point.
  views::Menu2::Alignment alignment;
};

// Returns true if the menu item type specified can be executed as a command.
bool MenuTypeCanExecute(ui::MenuModel::ItemType type) {
  return type == ui::MenuModel::TYPE_COMMAND ||
      type == ui::MenuModel::TYPE_CHECK ||
      type == ui::MenuModel::TYPE_RADIO;
}

// A callback to gtk_container_foreach to remove all children.
// See |NativeMenuGtk::ResetMenu| for the usage.
void RemoveChildWidget(GtkWidget* widget, gpointer data) {
  GtkWidget* parent = gtk_widget_get_parent(widget);
  gtk_container_remove(GTK_CONTAINER(parent), widget);
}

}  // namespace

namespace views {

////////////////////////////////////////////////////////////////////////////////
// NativeMenuGtk, public:

NativeMenuGtk::NativeMenuGtk(Menu2* menu)
    : parent_(NULL),
      model_(menu->model()),
      menu_(NULL),
      menu_hidden_(true),
      suppress_activate_signal_(false),
      activated_menu_(NULL),
      activated_index_(-1),
      activate_factory_(this),
      host_menu_(menu),
      menu_action_(MENU_ACTION_NONE),
      nested_dispatcher_(NULL),
      ignore_button_release_(true) {
}

NativeMenuGtk::~NativeMenuGtk() {
  if (nested_dispatcher_) {
    // Menu is destroyed while its in message loop.
    // Let nested dispatcher know the creator is deleted.
    nested_dispatcher_->CreatorDestroyed();
  }
  if (menu_) {
    // Don't call MenuDestroyed because menu2 has already been destroyed.
    g_signal_handler_disconnect(menu_, destroy_handler_id_);
    gtk_widget_destroy(menu_);
  }
}

////////////////////////////////////////////////////////////////////////////////
// NativeMenuGtk, MenuWrapper implementation:

void NativeMenuGtk::RunMenuAt(const gfx::Point& point, int alignment) {
  activated_menu_ = NULL;
  activated_index_ = -1;
  menu_action_ = MENU_ACTION_NONE;
  // ignore button release event unless mouse is pressed or moved.
  ignore_button_release_ = true;

  UpdateStates();
  Position position = { point, static_cast<Menu2::Alignment>(alignment) };
  // TODO(beng): value of '1' will not work for context menus!
  gtk_menu_popup(GTK_MENU(menu_), NULL, NULL, MenuPositionFunc, &position, 1,
                 gtk_get_current_event_time());
  DCHECK(menu_hidden_);
  menu_hidden_ = false;

  for (unsigned int i = 0; i < listeners_.size(); ++i) {
    listeners_[i]->OnMenuOpened();
  }

  // Listen for "hide" signal so that we know when to return from the blocking
  // RunMenuAt call.
  gint hide_handle_id =
      g_signal_connect(menu_, "hide", G_CALLBACK(OnMenuHiddenThunk), this);

  gint move_handle_id =
      g_signal_connect(menu_, "move-current",
                       G_CALLBACK(OnMenuMoveCurrentThunk), this);

  // Block until menu is no longer shown by running a nested message loop.
  nested_dispatcher_ = new NestedDispatcherGtk(this, true);
  bool deleted = nested_dispatcher_->RunAndSelfDestruct();
  if (deleted) {
    // The menu was destryed while menu is shown, so return immediately.
    // Don't touch the instance which is already deleted.
    return;
  }
  nested_dispatcher_ = NULL;
  if (!menu_hidden_) {
    // If this happens it means we haven't yet gotten the hide signal and
    // someone else quit the message loop on us.
    NOTREACHED();
    menu_hidden_ = true;
  }

  g_signal_handler_disconnect(G_OBJECT(menu_), hide_handle_id);
  g_signal_handler_disconnect(G_OBJECT(menu_), move_handle_id);

  if (activated_menu_) {
    MessageLoop::current()->PostTask(FROM_HERE,
                                     activate_factory_.NewRunnableMethod(
                                         &NativeMenuGtk::ProcessActivate));
  }
}

void NativeMenuGtk::CancelMenu() {
  gtk_widget_hide(menu_);
}

void NativeMenuGtk::Rebuild() {
  activated_menu_ = NULL;

  ResetMenu();

  // Try to retrieve accelerator group as data from menu_; if null, create new
  // one and store it as data into menu_.
  // We store it as data so as to use the destroy notifier to get rid of initial
  // reference count.  For some reason, when we unref it ourselves (even in
  // destructor), it would cause random crashes, depending on when gtk tries to
  // access it.
  GtkAccelGroup* accel_group = static_cast<GtkAccelGroup*>(
      g_object_get_data(G_OBJECT(menu_), kAccelGroupString));
  if (!accel_group) {
    accel_group = gtk_accel_group_new();
    g_object_set_data_full(G_OBJECT(menu_), kAccelGroupString, accel_group,
        g_object_unref);
  }

  std::map<int, GtkRadioMenuItem*> radio_groups_;
  for (int i = 0; i < model_->GetItemCount(); ++i) {
    ui::MenuModel::ItemType type = model_->GetTypeAt(i);
    if (type == ui::MenuModel::TYPE_SEPARATOR) {
      AddSeparatorAt(i);
    } else if (type == ui::MenuModel::TYPE_RADIO) {
      const int radio_group_id = model_->GetGroupIdAt(i);
      std::map<int, GtkRadioMenuItem*>::const_iterator iter
          = radio_groups_.find(radio_group_id);
      if (iter == radio_groups_.end()) {
        GtkWidget* new_menu_item = AddMenuItemAt(i, NULL, accel_group);
        // |new_menu_item| is the first menu item for |radio_group_id| group.
        radio_groups_.insert(
            std::make_pair(radio_group_id, GTK_RADIO_MENU_ITEM(new_menu_item)));
      } else {
        AddMenuItemAt(i, iter->second, accel_group);
      }
    } else {
      AddMenuItemAt(i, NULL, accel_group);
    }
  }
  if (!menu_hidden_)
    gtk_menu_reposition(GTK_MENU(menu_));
}

void NativeMenuGtk::UpdateStates() {
  gtk_container_foreach(GTK_CONTAINER(menu_), &UpdateStateCallback, this);
}

gfx::NativeMenu NativeMenuGtk::GetNativeMenu() const {
  return menu_;
}

NativeMenuGtk::MenuAction NativeMenuGtk::GetMenuAction() const {
  return menu_action_;
}

void NativeMenuGtk::AddMenuListener(MenuListener* listener) {
  listeners_.push_back(listener);
}

void NativeMenuGtk::RemoveMenuListener(MenuListener* listener) {
  for (std::vector<MenuListener*>::iterator iter = listeners_.begin();
    iter != listeners_.end();
    ++iter) {
      if (*iter == listener) {
        listeners_.erase(iter);
        return;
      }
  }
}

void NativeMenuGtk::SetMinimumWidth(int width) {
  gtk_widget_set_size_request(menu_, width, -1);
}

#if defined(TOUCH_UI)
bool NativeMenuGtk::Dispatch(XEvent* xevent) {
  return DispatchXEvent(xevent);
}
#endif

bool NativeMenuGtk::Dispatch(GdkEvent* event) {
  if (menu_hidden_) {
    // The menu has been closed but the message loop is still nested. Don't
    // dispatch a message, otherwise we might spawn another message loop.
    return false;  // Exits the nested message loop.
  }
  switch (event->type) {
    case GDK_BUTTON_PRESS:
    case GDK_2BUTTON_PRESS:
    case GDK_3BUTTON_PRESS: {
      ignore_button_release_ = false;
      gpointer data = NULL;
      gdk_window_get_user_data(((GdkEventAny*)event)->window, &data);
      GtkWidget* widget = reinterpret_cast<GtkWidget*>(data);
      if (widget) {
        GtkWidget* root = gtk_widget_get_toplevel(widget);
        if (!g_object_get_data(G_OBJECT(root), kNativeMenuGtkString)) {
          // The button event is not targeted at a menu, hide the menu and eat
          // the event. If we didn't do this the button press is dispatched from
          // the nested message loop and bad things can happen (like trying to
          // spawn another menu.
          gtk_menu_popdown(GTK_MENU(menu_));
          // In some cases we may not have gotten the hide event, but the menu
          // will be in the process of hiding so set menu_hidden_ anyway.
          menu_hidden_ = true;
          return false;  // Exits the nested message loop.
        }
      }
      break;
    }
    case GDK_MOTION_NOTIFY: {
      ignore_button_release_ = false;
      break;
    }
    case GDK_BUTTON_RELEASE: {
      if (ignore_button_release_) {
        // Ignore if a release event happened without press event.
        // Normally, release event is eaten by gtk when menu is opened
        // in response to mouse press event. Since the renderer opens
        // the context menu asyncrhonous after press event is handled,
        // gtk sometimes does not eat it, which causes the menu to be
        // closed.
        return true;
      }
      break;
    }
    default:
      break;
  }
  gtk_main_do_event(event);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// NativeMenuGtk, private:

void NativeMenuGtk::OnMenuHidden(GtkWidget* widget) {
  if (menu_hidden_) {
    // The menu has been already hidden by us and we're in the process of
    // quiting the message loop..
    return;
  }
  // Quit the nested message loop we spawned in RunMenuAt.
  MessageLoop::current()->Quit();
  menu_hidden_ = true;
}

void NativeMenuGtk::OnMenuMoveCurrent(GtkWidget* menu_widget,
                                      GtkMenuDirectionType focus_direction) {
  GtkWidget* parent = GTK_MENU_SHELL(menu_widget)->parent_menu_shell;
  GtkWidget* menu_item = GTK_MENU_SHELL(menu_widget)->active_menu_item;
  GtkWidget* submenu = NULL;
  if (menu_item) {
    submenu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(menu_item));
  }
  if (focus_direction == GTK_MENU_DIR_CHILD && submenu == NULL) {
    GetAncestor()->menu_action_ = MENU_ACTION_NEXT;
    gtk_menu_popdown(GTK_MENU(menu_widget));
  } else if (focus_direction == GTK_MENU_DIR_PARENT && parent == NULL) {
    GetAncestor()->menu_action_ = MENU_ACTION_PREVIOUS;
    gtk_menu_popdown(GTK_MENU(menu_widget));
  }
}

void NativeMenuGtk::AddSeparatorAt(int index) {
  GtkWidget* separator = gtk_separator_menu_item_new();
  gtk_widget_show(separator);
  gtk_menu_append(menu_, separator);
}

GtkWidget* NativeMenuGtk::AddMenuItemAt(int index,
                                        GtkRadioMenuItem* radio_group,
                                        GtkAccelGroup* accel_group) {
  GtkWidget* menu_item = NULL;
  std::string label = gfx::ConvertAcceleratorsFromWindowsStyle(UTF16ToUTF8(
      model_->GetLabelAt(index)));

  ui::MenuModel::ItemType type = model_->GetTypeAt(index);
  switch (type) {
    case ui::MenuModel::TYPE_CHECK:
      menu_item = gtk_check_menu_item_new_with_mnemonic(label.c_str());
      break;
    case ui::MenuModel::TYPE_RADIO:
      if (radio_group) {
        menu_item = gtk_radio_menu_item_new_with_mnemonic_from_widget(
            radio_group, label.c_str());
      } else {
        // The item does not belong to any existing radio button groups.
        menu_item = gtk_radio_menu_item_new_with_mnemonic(NULL, label.c_str());
      }
      break;
    case ui::MenuModel::TYPE_SUBMENU:
    case ui::MenuModel::TYPE_COMMAND: {
      SkBitmap icon;
      // Create menu item with icon if icon exists.
      if (model_->HasIcons() && model_->GetIconAt(index, &icon)) {
        menu_item = gtk_image_menu_item_new_with_mnemonic(label.c_str());
        GdkPixbuf* pixbuf = gfx::GdkPixbufFromSkBitmap(&icon);
        gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_item),
                                      gtk_image_new_from_pixbuf(pixbuf));
        g_object_unref(pixbuf);

#if GTK_CHECK_VERSION(2,16,0)
        // Show the image even if the "gtk-menu-images" setting is turned off.
        gtk_image_menu_item_set_always_show_image(
            GTK_IMAGE_MENU_ITEM(menu_item), TRUE);
#endif
      } else {
        menu_item = gtk_menu_item_new_with_mnemonic(label.c_str());
      }
      break;
    }
    default:
      NOTREACHED();
      break;
  }

  // Label font.
  const gfx::Font* font = model_->GetLabelFontAt(index);
  if (font) {
    // The label item is the first child of the menu item.
    GtkWidget* label_widget = GTK_BIN(menu_item)->child;
    DCHECK(label_widget && GTK_IS_LABEL(label_widget));
    PangoFontDescription* pfd = font->GetNativeFont();
    gtk_widget_modify_font(label_widget, pfd);
    pango_font_description_free(pfd);
  }

  if (type == ui::MenuModel::TYPE_SUBMENU) {
    Menu2* submenu = new Menu2(model_->GetSubmenuModelAt(index));
    static_cast<NativeMenuGtk*>(submenu->wrapper_.get())->set_parent(this);
    g_object_set_data(G_OBJECT(menu_item), "submenu", submenu);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item),
                              submenu->GetNativeMenu());
  }

  views::Accelerator accelerator(ui::VKEY_UNKNOWN, false, false, false);
  if (accel_group && model_->GetAcceleratorAt(index, &accelerator)) {
    int gdk_modifiers = 0;
    if (accelerator.IsShiftDown())
      gdk_modifiers |= GDK_SHIFT_MASK;
    if (accelerator.IsCtrlDown())
      gdk_modifiers |= GDK_CONTROL_MASK;
    if (accelerator.IsAltDown())
      gdk_modifiers |= GDK_MOD1_MASK;
    gtk_widget_add_accelerator(menu_item, "activate", accel_group,
        ui::GdkKeyCodeForWindowsKeyCode(accelerator.GetKeyCode(), false),
        static_cast<GdkModifierType>(gdk_modifiers), GTK_ACCEL_VISIBLE);
  }

  g_object_set_data(G_OBJECT(menu_item), kPositionString,
                             reinterpret_cast<void*>(index));
  g_signal_connect(menu_item, "activate", G_CALLBACK(CallActivate), this);
  UpdateMenuItemState(menu_item, false);
  gtk_widget_show(menu_item);
  gtk_menu_append(menu_, menu_item);

  return menu_item;
}

void NativeMenuGtk::ResetMenu() {
  if (!menu_) {
    menu_ = gtk_menu_new();
    g_object_set_data(
        G_OBJECT(GTK_MENU(menu_)->toplevel), kNativeMenuGtkString, this);
    destroy_handler_id_ = g_signal_connect(
        menu_, "destroy", G_CALLBACK(NativeMenuGtk::MenuDestroyed), host_menu_);
  } else {
    gtk_container_foreach(GTK_CONTAINER(menu_), RemoveChildWidget, NULL);
  }
}

void NativeMenuGtk::UpdateMenuItemState(GtkWidget* menu_item, bool recurse) {
  int index = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(menu_item),
                                                kPositionString));

  gtk_widget_set_sensitive(menu_item, model_->IsEnabledAt(index));
  if (GTK_IS_CHECK_MENU_ITEM(menu_item)) {
    suppress_activate_signal_ = true;
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_item),
                                   model_->IsItemCheckedAt(index));
    suppress_activate_signal_ = false;
  }

  if (recurse && GTK_IS_MENU_ITEM(menu_item)) {
    // Recurse into submenus.
    if (gtk_menu_item_get_submenu(GTK_MENU_ITEM(menu_item))) {
      Menu2* submenu =
          reinterpret_cast<Menu2*>(g_object_get_data(G_OBJECT(menu_item),
                                   "submenu"));
      if (submenu)
        submenu->UpdateStates();
    }
  }
}

// static
void NativeMenuGtk::UpdateStateCallback(GtkWidget* menu_item, gpointer data) {
  NativeMenuGtk* menu = reinterpret_cast<NativeMenuGtk*>(data);
  menu->UpdateMenuItemState(menu_item, true);
}

// static
void NativeMenuGtk::MenuPositionFunc(GtkMenu* menu,
                                     int* x,
                                     int* y,
                                     gboolean* push_in,
                                     void* data) {
  Position* position = reinterpret_cast<Position*>(data);

  GtkRequisition menu_req;
  gtk_widget_size_request(GTK_WIDGET(menu), &menu_req);

  *x = position->point.x();
  *y = position->point.y();
  views::Menu2::Alignment alignment = position->alignment;
  if (base::i18n::IsRTL()) {
    switch (alignment) {
      case Menu2::ALIGN_TOPRIGHT:
        alignment = Menu2::ALIGN_TOPLEFT;
        break;
      case Menu2::ALIGN_TOPLEFT:
        alignment = Menu2::ALIGN_TOPRIGHT;
        break;
      default:
        NOTREACHED();
        break;
    }
  }
  if (alignment == Menu2::ALIGN_TOPRIGHT)
    *x -= menu_req.width;

  // Make sure the popup fits on screen.
  GdkScreen* screen = gtk_widget_get_screen(GTK_WIDGET(menu));
  *x = std::max(0, std::min(gdk_screen_get_width(screen) - menu_req.width, *x));
  *y = std::max(0, std::min(gdk_screen_get_height(screen) - menu_req.height,
                            *y));

  *push_in = FALSE;
}

void NativeMenuGtk::OnActivate(GtkMenuItem* menu_item) {
  if (suppress_activate_signal_)
    return;
  int position = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(menu_item),
                                                   kPositionString));
  // Ignore the signal if it's sent to an inactive checked radio item.
  //
  // Suppose there are three radio items A, B, C, and A is now being
  // checked. If you click C, "activate" signal will be sent to A and C.
  // Here, we ignore the signal sent to A.
  if (GTK_IS_RADIO_MENU_ITEM(menu_item) &&
      !gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menu_item))) {
    return;
  }

  // NOTE: we get activate messages for submenus when first shown.
  if (model_->IsEnabledAt(position) &&
      MenuTypeCanExecute(model_->GetTypeAt(position))) {
    NativeMenuGtk* ancestor = GetAncestor();
    ancestor->activated_menu_ = this;
    activated_index_ = position;
    ancestor->menu_action_ = MENU_ACTION_SELECTED;
  }
}

// static
void NativeMenuGtk::CallActivate(GtkMenuItem* menu_item,
                                 NativeMenuGtk* native_menu) {
  native_menu->OnActivate(menu_item);
}

NativeMenuGtk* NativeMenuGtk::GetAncestor() {
  NativeMenuGtk* ancestor = this;
  while (ancestor->parent_)
    ancestor = ancestor->parent_;
  return ancestor;
}

void NativeMenuGtk::ProcessActivate() {
  if (activated_menu_)
    activated_menu_->Activate();
}

void NativeMenuGtk::Activate() {
  if (model_->IsEnabledAt(activated_index_) &&
      MenuTypeCanExecute(model_->GetTypeAt(activated_index_))) {
    model_->ActivatedAt(activated_index_);
  }
}

// static
void NativeMenuGtk::MenuDestroyed(GtkWidget* widget, Menu2* menu2) {
  NativeMenuGtk* native_menu =
      static_cast<NativeMenuGtk*>(menu2->wrapper_.get());
  // The native gtk widget has already been destroyed.
  native_menu->menu_ = NULL;
  delete menu2;
}

////////////////////////////////////////////////////////////////////////////////
// MenuWrapper, public:

// static
MenuWrapper* MenuWrapper::CreateWrapper(Menu2* menu) {
  return new NativeMenuGtk(menu);
}

}  // namespace views
