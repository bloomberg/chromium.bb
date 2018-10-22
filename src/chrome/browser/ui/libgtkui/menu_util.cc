// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/libgtkui/menu_util.h"

#include <map>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/libgtkui/gtk_util.h"
#include "chrome/browser/ui/libgtkui/skia_utils_gtk.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/menu_label_accelerator_util_linux.h"
#include "ui/base/models/menu_model.h"

namespace libgtkui {

GtkWidget* BuildMenuItemWithImage(const std::string& label, GtkWidget* image) {
// GTK4 removed support for image menu items.
#if GTK_CHECK_VERSION(3, 90, 0)
  return gtk_menu_item_new_with_mnemonic(label.c_str());
#else
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  GtkWidget* menu_item = gtk_image_menu_item_new_with_mnemonic(label.c_str());
  gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_item), image);
  G_GNUC_END_IGNORE_DEPRECATIONS;
  return menu_item;
#endif
}

GtkWidget* BuildMenuItemWithImage(const std::string& label,
                                  const gfx::Image& icon) {
  GdkPixbuf* pixbuf = GdkPixbufFromSkBitmap(*icon.ToSkBitmap());

  GtkWidget* menu_item =
      BuildMenuItemWithImage(label, gtk_image_new_from_pixbuf(pixbuf));
  g_object_unref(pixbuf);
  return menu_item;
}

GtkWidget* BuildMenuItemWithLabel(const std::string& label) {
  return gtk_menu_item_new_with_mnemonic(label.c_str());
}

ui::MenuModel* ModelForMenuItem(GtkMenuItem* menu_item) {
  return reinterpret_cast<ui::MenuModel*>(
      g_object_get_data(G_OBJECT(menu_item), "model"));
}

GtkWidget* AppendMenuItemToMenu(int index,
                                ui::MenuModel* model,
                                GtkWidget* menu_item,
                                GtkWidget* menu,
                                bool connect_to_activate,
                                GCallback item_activated_cb,
                                void* this_ptr) {
  // Set the ID of a menu item.
  // Add 1 to the menu_id to avoid setting zero (null) to "menu-id".
  g_object_set_data(G_OBJECT(menu_item), "menu-id", GINT_TO_POINTER(index + 1));

  // Native menu items do their own thing, so only selectively listen for the
  // activate signal.
  if (connect_to_activate) {
    g_signal_connect(menu_item, "activate", item_activated_cb, this_ptr);
  }

  // AppendMenuItemToMenu is used both internally when we control menu creation
  // from a model (where the model can choose to hide certain menu items), and
  // with immediate commands which don't provide the option.
  if (model) {
    if (model->IsVisibleAt(index))
      gtk_widget_show(menu_item);
  } else {
    gtk_widget_show(menu_item);
  }
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
  return menu_item;
}

bool GetMenuItemID(GtkWidget* menu_item, int* menu_id) {
  gpointer id_ptr = g_object_get_data(G_OBJECT(menu_item), "menu-id");
  if (id_ptr != nullptr) {
    *menu_id = GPOINTER_TO_INT(id_ptr) - 1;
    return true;
  }

  return false;
}

void ExecuteCommand(ui::MenuModel* model, int id) {
  GdkEvent* event = gtk_get_current_event();
  int event_flags = 0;

  if (event && event->type == GDK_BUTTON_RELEASE)
    event_flags = EventFlagsFromGdkState(event->button.state);
  model->ActivatedAt(id, event_flags);

  if (event)
    gdk_event_free(event);
}

void BuildSubmenuFromModel(ui::MenuModel* model,
                           GtkWidget* menu,
                           GCallback item_activated_cb,
                           bool* block_activation,
                           void* this_ptr) {
  std::map<int, GtkWidget*> radio_groups;
  GtkWidget* menu_item = nullptr;
  for (int i = 0; i < model->GetItemCount(); ++i) {
    gfx::Image icon;
    std::string label = ui::ConvertAcceleratorsFromWindowsStyle(
        base::UTF16ToUTF8(model->GetLabelAt(i)));

    bool connect_to_activate = true;

    switch (model->GetTypeAt(i)) {
      case ui::MenuModel::TYPE_SEPARATOR:
        menu_item = gtk_separator_menu_item_new();
        break;

      case ui::MenuModel::TYPE_CHECK:
        menu_item = gtk_check_menu_item_new_with_mnemonic(label.c_str());
        break;

      case ui::MenuModel::TYPE_RADIO: {
        std::map<int, GtkWidget*>::iterator iter =
            radio_groups.find(model->GetGroupIdAt(i));

        if (iter == radio_groups.end()) {
          menu_item =
              gtk_radio_menu_item_new_with_mnemonic(nullptr, label.c_str());
          radio_groups[model->GetGroupIdAt(i)] = menu_item;
        } else {
          menu_item = gtk_radio_menu_item_new_with_mnemonic_from_widget(
              GTK_RADIO_MENU_ITEM(iter->second), label.c_str());
        }
        break;
      }
      case ui::MenuModel::TYPE_BUTTON_ITEM: {
        NOTIMPLEMENTED();
        break;
      }
      case ui::MenuModel::TYPE_SUBMENU:
      case ui::MenuModel::TYPE_COMMAND: {
        if (model->GetIconAt(i, &icon))
          menu_item = BuildMenuItemWithImage(label, icon);
        else
          menu_item = BuildMenuItemWithLabel(label);
#if !GTK_CHECK_VERSION(3, 90, 0)
        G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
        if (GTK_IS_IMAGE_MENU_ITEM(menu_item)) {
          gtk_image_menu_item_set_always_show_image(
              GTK_IMAGE_MENU_ITEM(menu_item), TRUE);
        }
        G_GNUC_END_IGNORE_DEPRECATIONS;
#endif
        break;
      }

      default:
        NOTREACHED();
    }

    if (model->GetTypeAt(i) == ui::MenuModel::TYPE_SUBMENU) {
      GtkWidget* submenu = gtk_menu_new();
      ui::MenuModel* submenu_model = model->GetSubmenuModelAt(i);
      BuildSubmenuFromModel(submenu_model,
                            submenu,
                            item_activated_cb,
                            block_activation,
                            this_ptr);
      gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), submenu);

      // Update all the menu item info in the newly-generated menu.
      gtk_container_foreach(
          GTK_CONTAINER(submenu), SetMenuItemInfo, block_activation);
      submenu_model->MenuWillShow();
      connect_to_activate = false;
    }

    ui::Accelerator accelerator;
    if (model->GetAcceleratorAt(i, &accelerator)) {
      gtk_widget_add_accelerator(menu_item, "activate", nullptr,
                                 GetGdkKeyCodeForAccelerator(accelerator),
                                 GetGdkModifierForAccelerator(accelerator),
                                 GTK_ACCEL_VISIBLE);
    }

    g_object_set_data(G_OBJECT(menu_item), "model", model);
    AppendMenuItemToMenu(i,
                         model,
                         menu_item,
                         menu,
                         connect_to_activate,
                         item_activated_cb,
                         this_ptr);

    menu_item = nullptr;
  }
}

void SetMenuItemInfo(GtkWidget* widget, void* block_activation_ptr) {
  if (GTK_IS_SEPARATOR_MENU_ITEM(widget)) {
    // We need to explicitly handle this case because otherwise we'll ask the
    // menu delegate about something with an invalid id.
    return;
  }

  int id;
  if (!GetMenuItemID(widget, &id))
    return;

  ui::MenuModel* model = ModelForMenuItem(GTK_MENU_ITEM(widget));
  if (!model) {
    // If we're not providing the sub menu, then there's no model.  For
    // example, the IME submenu doesn't have a model.
    return;
  }
  bool* block_activation = static_cast<bool*>(block_activation_ptr);

  if (GTK_IS_CHECK_MENU_ITEM(widget)) {
    GtkCheckMenuItem* item = GTK_CHECK_MENU_ITEM(widget);

    // gtk_check_menu_item_set_active() will send the activate signal. Touching
    // the underlying "active" property will also call the "activate" handler
    // for this menu item. So we prevent the "activate" handler from
    // being called while we set the checkbox.
    // Why not use one of the glib signal-blocking functions?  Because when we
    // toggle a radio button, it will deactivate one of the other radio buttons,
    // which we don't have a pointer to.
    *block_activation = true;
    gtk_check_menu_item_set_active(item, model->IsItemCheckedAt(id));
    *block_activation = false;
  }

  if (GTK_IS_MENU_ITEM(widget)) {
    gtk_widget_set_sensitive(widget, model->IsEnabledAt(id));

    if (model->IsVisibleAt(id)) {
      // Update the menu item label if it is dynamic.
      if (model->IsItemDynamicAt(id)) {
        std::string label = ui::ConvertAcceleratorsFromWindowsStyle(
            base::UTF16ToUTF8(model->GetLabelAt(id)));

        gtk_menu_item_set_label(GTK_MENU_ITEM(widget), label.c_str());
#if !GTK_CHECK_VERSION(3, 90, 0)
        G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
        if (GTK_IS_IMAGE_MENU_ITEM(widget)) {
          gfx::Image icon;
          if (model->GetIconAt(id, &icon)) {
            GdkPixbuf* pixbuf = GdkPixbufFromSkBitmap(*icon.ToSkBitmap());
            gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(widget),
                                          gtk_image_new_from_pixbuf(pixbuf));
            g_object_unref(pixbuf);
          } else {
            gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(widget), nullptr);
          }
        }
        G_GNUC_END_IGNORE_DEPRECATIONS;
#endif
      }

      gtk_widget_show(widget);
    } else {
      gtk_widget_hide(widget);
    }

    GtkWidget* submenu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(widget));
    if (submenu) {
      gtk_container_foreach(
          GTK_CONTAINER(submenu), &SetMenuItemInfo, block_activation_ptr);
    }
  }
}

}  // namespace libgtkui
