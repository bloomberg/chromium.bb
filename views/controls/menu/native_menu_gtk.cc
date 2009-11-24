// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "views/controls/menu/native_menu_gtk.h"

#include <map>
#include <string>

#include "app/gfx/gtk_util.h"
#include "base/keyboard_codes.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/time.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "views/accelerator.h"
#include "views/controls/menu/menu_2.h"

namespace {

const char kPositionString[] = "position";

// Data passed to the MenuPositionFunc from gtk_menu_popup
struct Position {
  // The point to run the menu at.
  gfx::Point point;
  // The alignment of the menu at that point.
  views::Menu2::Alignment alignment;
};

std::string ConvertAcceleratorsFromWindowsStyle(const std::string& label) {
  std::string ret;
  ret.reserve(label.length());
  for (size_t i = 0; i < label.length(); ++i) {
    if ('&' == label[i]) {
      if (i + 1 < label.length() && '&' == label[i + 1]) {
        ret.push_back(label[i]);
        ++i;
      } else {
        ret.push_back('_');
      }
    } else {
      ret.push_back(label[i]);
    }
  }

  return ret;
}

// Returns true if the menu item type specified can be executed as a command.
bool MenuTypeCanExecute(views::Menu2Model::ItemType type) {
  return type == views::Menu2Model::TYPE_COMMAND ||
      type == views::Menu2Model::TYPE_CHECK ||
      type == views::Menu2Model::TYPE_RADIO;
}

}  // namespace

namespace views {

////////////////////////////////////////////////////////////////////////////////
// NativeMenuGtk, public:

NativeMenuGtk::NativeMenuGtk(Menu2Model* model)
    : model_(model),
      menu_(NULL),
      menu_shown_(false),
      suppress_activate_signal_(false) {
}

NativeMenuGtk::~NativeMenuGtk() {
  gtk_widget_destroy(menu_);
}

////////////////////////////////////////////////////////////////////////////////
// NativeMenuGtk, MenuWrapper implementation:

void NativeMenuGtk::RunMenuAt(const gfx::Point& point, int alignment) {
  UpdateStates();
  Position position = { point, static_cast<Menu2::Alignment>(alignment) };
  // TODO(beng): value of '1' will not work for context menus!
  gtk_menu_popup(GTK_MENU(menu_), NULL, NULL, MenuPositionFunc, &position, 1,
                 gtk_get_current_event_time());

  DCHECK(!menu_shown_);
  menu_shown_ = true;
  // Listen for "hide" signal so that we know when to return from the blocking
  // RunMenuAt call.
  gint handle_id =
      g_signal_connect(G_OBJECT(menu_), "hide", G_CALLBACK(OnMenuHidden), this);

  // Block until menu is no longer shown by running a nested message loop.
  MessageLoopForUI::current()->Run(NULL);

  g_signal_handler_disconnect(G_OBJECT(menu_), handle_id);
  menu_shown_ = false;
}

void NativeMenuGtk::CancelMenu() {
  NOTIMPLEMENTED();
}

void NativeMenuGtk::Rebuild() {
  ResetMenu();

  std::map<int, GtkRadioMenuItem*> radio_groups_;
  for (int i = 0; i < model_->GetItemCount(); ++i) {
    Menu2Model::ItemType type = model_->GetTypeAt(i);
    if (type == Menu2Model::TYPE_SEPARATOR) {
      AddSeparatorAt(i);
    } else if (type == Menu2Model::TYPE_RADIO) {
      const int radio_group_id = model_->GetGroupIdAt(i);
      std::map<int, GtkRadioMenuItem*>::const_iterator iter
          = radio_groups_.find(radio_group_id);
      if (iter == radio_groups_.end()) {
        GtkWidget* new_menu_item = AddMenuItemAt(i, NULL);
        // |new_menu_item| is the first menu item for |radio_group_id| group.
        radio_groups_.insert(
            std::make_pair(radio_group_id, GTK_RADIO_MENU_ITEM(new_menu_item)));
      } else {
        AddMenuItemAt(i, iter->second);
      }
    } else {
      AddMenuItemAt(i, NULL);
    }
  }
}

void NativeMenuGtk::UpdateStates() {
  gtk_container_foreach(GTK_CONTAINER(menu_), &UpdateStateCallback, this);
}

gfx::NativeMenu NativeMenuGtk::GetNativeMenu() const {
  return menu_;
}

////////////////////////////////////////////////////////////////////////////////
// NativeMenuGtk, private:

// static
void NativeMenuGtk::OnMenuHidden(GtkWidget* widget, NativeMenuGtk* menu) {
  if (!menu->menu_shown_) {
    // This indicates we don't have a menu open, and should never happen.
    NOTREACHED();
    return;
  }
  // Quit the nested message loop we spawned in RunMenuAt.
  MessageLoop::current()->Quit();
}

void NativeMenuGtk::AddSeparatorAt(int index) {
  GtkWidget* separator = gtk_separator_menu_item_new();
  gtk_widget_show(separator);
  gtk_menu_append(menu_, separator);
}

GtkWidget* NativeMenuGtk::AddMenuItemAt(int index,
                                        GtkRadioMenuItem* radio_group) {
  GtkWidget* menu_item = NULL;
  std::string label = ConvertAcceleratorsFromWindowsStyle(UTF16ToUTF8(
      model_->GetLabelAt(index)));

  Menu2Model::ItemType type = model_->GetTypeAt(index);
  switch (type) {
    case Menu2Model::TYPE_CHECK:
      menu_item = gtk_check_menu_item_new_with_mnemonic(label.c_str());
      break;
    case Menu2Model::TYPE_RADIO:
      if (radio_group) {
        menu_item = gtk_radio_menu_item_new_with_mnemonic_from_widget(
            radio_group, label.c_str());
      } else {
        // The item does not belong to any existing radio button groups.
        menu_item = gtk_radio_menu_item_new_with_mnemonic(NULL, label.c_str());
      }
      break;
    case Menu2Model::TYPE_SUBMENU:
    case Menu2Model::TYPE_COMMAND: {
      SkBitmap icon;
      // Create menu item with icon if icon exists.
      if (model_->HasIcons() && model_->GetIconAt(index, &icon)) {
        menu_item = gtk_image_menu_item_new_with_mnemonic(label.c_str());
        GdkPixbuf* pixbuf = gfx::GdkPixbufFromSkBitmap(&icon);
        gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_item),
                                      gtk_image_new_from_pixbuf(pixbuf));
      } else {
        menu_item = gtk_menu_item_new_with_mnemonic(label.c_str());
      }
      break;
    }
    default:
      NOTREACHED();
      break;
  }

  if (type == Menu2Model::TYPE_SUBMENU) {
    // TODO(beng): we're leaking these objects right now... consider some other
    //             arrangement.
    Menu2* submenu = new Menu2(model_->GetSubmenuModelAt(index));
    g_object_set_data(G_OBJECT(menu_item), "submenu", submenu);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item),
                              submenu->GetNativeMenu());
  }

  views::Accelerator accelerator(base::VKEY_UNKNOWN, false, false, false);
  if (model_->GetAcceleratorAt(index, &accelerator)) {
    // TODO(beng): accelerators w/gtk_widget_add_accelerator.
  }
  g_object_set_data(G_OBJECT(menu_item), kPositionString,
                             reinterpret_cast<void*>(index));
  g_signal_connect(G_OBJECT(menu_item), "activate", G_CALLBACK(CallActivate),
                   this);
  gtk_widget_show(menu_item);
  gtk_menu_append(menu_, menu_item);

  return menu_item;
}

void NativeMenuGtk::ResetMenu() {
  if (menu_)
    gtk_widget_destroy(menu_);
  menu_ = gtk_menu_new();
}

void NativeMenuGtk::UpdateMenuItemState(GtkWidget* menu_item) {
  int index = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(menu_item),
                                                kPositionString));

  gtk_widget_set_sensitive(menu_item, model_->IsEnabledAt(index));
  if (GTK_IS_CHECK_MENU_ITEM(menu_item)) {
    suppress_activate_signal_ = true;
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_item),
                                   model_->IsItemCheckedAt(index));
    suppress_activate_signal_ = false;
  }
  // Recurse into submenus, too.
  if (GTK_IS_MENU_ITEM(menu_item)) {
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
  menu->UpdateMenuItemState(menu_item);
}

// static
void NativeMenuGtk::MenuPositionFunc(GtkMenu* menu,
                                     int* x,
                                     int* y,
                                     gboolean* push_in,
                                     void* data) {
  Position* position = reinterpret_cast<Position*>(data);
  // TODO(beng): RTL
  *x = position->point.x();
  *y = position->point.y();
  if (position->alignment == Menu2::ALIGN_TOPRIGHT) {
    GtkRequisition menu_req;
    gtk_widget_size_request(GTK_WIDGET(menu), &menu_req);
    *x -= menu_req.width;
  }
  *push_in = FALSE;
}

void NativeMenuGtk::OnActivate(GtkMenuItem* menu_item) {
  if (suppress_activate_signal_)
    return;
  int position = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(menu_item),
                                                   kPositionString));
  if (model_->IsEnabledAt(position) &&
      MenuTypeCanExecute(model_->GetTypeAt(position))) {
    model_->ActivatedAt(position);
  }
}

// static
void NativeMenuGtk::CallActivate(GtkMenuItem* menu_item,
                                 NativeMenuGtk* native_menu) {
  native_menu->OnActivate(menu_item);
}

////////////////////////////////////////////////////////////////////////////////
// MenuWrapper, public:

// static
MenuWrapper* MenuWrapper::CreateWrapper(Menu2* menu) {
  return new NativeMenuGtk(menu->model());
}

}  // namespace views
