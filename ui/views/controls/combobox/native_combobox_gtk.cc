// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/combobox/native_combobox_gtk.h"

#include <gtk/gtk.h>

#include <algorithm>

#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "ui/base/models/combobox_model.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/combobox/native_combobox_views.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/widget.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// NativeComboboxGtk, public:

NativeComboboxGtk::NativeComboboxGtk(Combobox* combobox)
    : combobox_(combobox),
      menu_(NULL) {
  set_focus_view(combobox);
}

NativeComboboxGtk::~NativeComboboxGtk() {
}

////////////////////////////////////////////////////////////////////////////////
// NativeComboboxGtk, NativeComboboxWrapper implementation:

void NativeComboboxGtk::UpdateFromModel() {
  if (!native_view())
    return;

  preferred_size_ = gfx::Size();

  GtkListStore* store =
      GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(native_view())));
  ui::ComboboxModel* model = combobox_->model();
  int count = model->GetItemCount();
  gtk_list_store_clear(store);
  GtkTreeIter iter;
  while (count-- > 0) {
    gtk_list_store_prepend(store, &iter);
    gtk_list_store_set(store, &iter,
                       0, UTF16ToUTF8(model->GetItemAt(count)).c_str(),
                       -1);
  }
}

void NativeComboboxGtk::UpdateSelectedItem() {
  if (!native_view())
    return;
  gtk_combo_box_set_active(
      GTK_COMBO_BOX(native_view()), combobox_->selected_item());
}

void NativeComboboxGtk::UpdateEnabled() {
  SetEnabled(combobox_->IsEnabled());
}

int NativeComboboxGtk::GetSelectedItem() const {
  if (!native_view())
    return 0;
  return gtk_combo_box_get_active(GTK_COMBO_BOX(native_view()));
}

bool NativeComboboxGtk::IsDropdownOpen() const {
  if (!native_view())
    return false;
  gboolean popup_shown;
  g_object_get(G_OBJECT(native_view()), "popup-shown", &popup_shown, NULL);
  return popup_shown;
}

gfx::Size NativeComboboxGtk::GetPreferredSize() {
  if (!native_view())
    return gfx::Size();

  if (preferred_size_.IsEmpty()) {
    GtkRequisition size_request = { 0, 0 };
    gtk_widget_size_request(native_view(), &size_request);
    // TODO(oshima|scott): we may not need ::max to 29. revisit this.
    preferred_size_.SetSize(size_request.width,
                            std::max(size_request.height, 29));
  }
  return preferred_size_;
}

View* NativeComboboxGtk::GetView() {
  return this;
}

void NativeComboboxGtk::SetFocus() {
  OnFocus();
}

bool NativeComboboxGtk::HandleKeyPressed(const views::KeyEvent& event) {
  return false;
}

bool NativeComboboxGtk::HandleKeyReleased(const views::KeyEvent& event) {
  return false;
}

void NativeComboboxGtk::HandleFocus() {
}

void NativeComboboxGtk::HandleBlur() {
}

gfx::NativeView NativeComboboxGtk::GetTestingHandle() const {
  return native_view();
}


////////////////////////////////////////////////////////////////////////////////
// NativeComboboxGtk, NativeControlGtk overrides:
void NativeComboboxGtk::CreateNativeControl() {
  GtkListStore* store = gtk_list_store_new(1, G_TYPE_STRING);
  GtkWidget* widget = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
  g_object_unref(G_OBJECT(store));

  GtkCellRenderer* cell = gtk_cell_renderer_text_new();
  gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(widget), cell, TRUE);
  gtk_cell_layout_set_attributes(
      GTK_CELL_LAYOUT(widget), cell, "text", 0, NULL);
  g_signal_connect(widget, "changed",
                   G_CALLBACK(CallChangedThunk), this);
  g_signal_connect_after(widget, "popup",
                         G_CALLBACK(CallPopUpThunk), this);

  // Get the menu associated with the combo box and listen to events on it.
  GList* menu_list = gtk_menu_get_for_attach_widget(widget);
  if (menu_list) {
    menu_ = reinterpret_cast<GtkMenu*>(menu_list->data);
    g_signal_connect_after(menu_, "move-current",
                           G_CALLBACK(CallMenuMoveCurrentThunk), this);
  }

  NativeControlCreated(widget);
}

void NativeComboboxGtk::NativeControlCreated(GtkWidget* native_control) {
  NativeControlGtk::NativeControlCreated(native_control);
  // Set the initial state of the combobox.
  UpdateFromModel();
  UpdateEnabled();
  UpdateSelectedItem();
}

////////////////////////////////////////////////////////////////////////////////
// NativeComboboxGtk, private:
void NativeComboboxGtk::SelectionChanged() {
  combobox_->SelectionChanged();
}

void NativeComboboxGtk::FocusedMenuItemChanged() {
  DCHECK(menu_);
  GtkWidget* menu_item = GTK_MENU_SHELL(menu_)->active_menu_item;
  if (!menu_item)
    return;

  // Figure out the item index and total number of items.
  GList* items = gtk_container_get_children(GTK_CONTAINER(menu_));
  guint count = g_list_length(items);
  int index = g_list_index(items, static_cast<gconstpointer>(menu_item));

  // Get the menu item's label.
  std::string name;
  GList* children = gtk_container_get_children(GTK_CONTAINER(menu_item));
  for (GList* l = g_list_first(children); l != NULL; l = g_list_next(l)) {
    GtkWidget* child = static_cast<GtkWidget*>(l->data);
    if (GTK_IS_CELL_VIEW(child)) {
      GtkCellView* cell_view = GTK_CELL_VIEW(child);
      GtkTreePath* path = gtk_cell_view_get_displayed_row(cell_view);
      GtkTreeModel* model = NULL;
      model = gtk_cell_view_get_model(cell_view);
      GtkTreeIter iter;
      if (model && gtk_tree_model_get_iter(model, &iter, path)) {
        GValue value = { 0 };
        gtk_tree_model_get_value(model, &iter, 0, &value);
        name = g_value_get_string(&value);
        break;
      }
    }
  }

  if (ViewsDelegate::views_delegate) {
    ViewsDelegate::views_delegate->NotifyMenuItemFocused(string16(),
                                                         UTF8ToUTF16(name),
                                                         index,
                                                         count,
                                                         false);
  }
}

void NativeComboboxGtk::CallChanged(GtkWidget* widget) {
  SelectionChanged();
}

gboolean NativeComboboxGtk::CallPopUp(GtkWidget* widget) {
  FocusedMenuItemChanged();
  return false;
}

void NativeComboboxGtk::CallMenuMoveCurrent(
    GtkWidget* menu, GtkMenuDirectionType focus_direction) {
  FocusedMenuItemChanged();
}

////////////////////////////////////////////////////////////////////////////////
// NativeComboboxWrapper, public:

// static
NativeComboboxWrapper* NativeComboboxWrapper::CreateWrapper(
    Combobox* combobox) {
  if (Widget::IsPureViews())
    return new NativeComboboxViews(combobox);
  return new NativeComboboxGtk(combobox);
}

}  // namespace views
