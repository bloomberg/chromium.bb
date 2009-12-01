// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "views/controls/table/native_table_gtk.h"

#include "app/gfx/gtk_util.h"
#include "base/string_util.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "views/controls/table/table_view2.h"
#include "views/controls/table/table_view_observer.h"
#include "views/widget/widget.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// NativeTableGtk, public:

NativeTableGtk::NativeTableGtk(TableView2* table)
    : table_(table),
      gtk_model_(NULL),
      tree_view_(NULL),
      tree_selection_(NULL) {
  // Associates the actual GtkWidget with the table so the table is the one
  // considered as having the focus (not the wrapper) when the HWND is
  // focused directly (with a click for example).
  set_focus_view(table);
}

NativeTableGtk::~NativeTableGtk() {
}

////////////////////////////////////////////////////////////////////////////////
// NativeTableGtk, NativeTableWrapper implementation:

int NativeTableGtk::GetRowCount() const {
  if (!tree_view_)
    return 0;

  GtkTreeIter iter;
  if (!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(gtk_model_), &iter))
    return 0;  // Empty tree.

  int count = 1;
  while (gtk_tree_model_iter_next(GTK_TREE_MODEL(gtk_model_), &iter))
    count++;
  return count;
}

View* NativeTableGtk::GetView() {
  return this;
}

void NativeTableGtk::SetFocus() {
  // Focus the associated widget.
  Focus();
}

gfx::NativeView NativeTableGtk::GetTestingHandle() const {
  // Note that we are returning the tree view, not the scrolled window as
  // arguably the tests need to access the tree view.
  return GTK_WIDGET(tree_view_);
}

void NativeTableGtk::InsertColumn(const TableColumn& column, int index) {
  NOTIMPLEMENTED();
}

void NativeTableGtk::RemoveColumn(int index) {
  NOTIMPLEMENTED();
}

int NativeTableGtk::GetColumnWidth(int column_index) const {
  NOTIMPLEMENTED();
  return -1;
}

void NativeTableGtk::SetColumnWidth(int column_index, int width) {
  NOTIMPLEMENTED();
}

int NativeTableGtk::GetSelectedRowCount() const {
  return gtk_tree_selection_count_selected_rows(tree_selection_);
}

int NativeTableGtk::GetFirstSelectedRow() const {
  int result = -1;
  GList* selected_rows =
      gtk_tree_selection_get_selected_rows(tree_selection_, NULL);
  if (g_list_length(selected_rows) > 0) {
    GtkTreePath* tree_path =
        static_cast<GtkTreePath*>(g_list_first(selected_rows)->data);
    gint* indices = gtk_tree_path_get_indices(tree_path);
    CHECK(indices);
    result = indices[0];
  }

  g_list_foreach(selected_rows, reinterpret_cast<GFunc>(gtk_tree_path_free),
                 NULL);
  g_list_free(selected_rows);
  return result;
}

int NativeTableGtk::GetFirstFocusedRow() const {
  NOTIMPLEMENTED();
  return -1;
}

bool NativeTableGtk::IsRowFocused(int model_row) const {
  NOTIMPLEMENTED();
  return false;
}

void NativeTableGtk::ClearRowFocus() {
  NOTIMPLEMENTED();
}

void NativeTableGtk::ClearSelection() {
  gtk_tree_selection_unselect_all(tree_selection_);
}

void NativeTableGtk::SetSelectedState(int model_row, bool state) {
  GtkTreeIter iter;
  if (!gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(gtk_model_), &iter, NULL,
                                     model_row)) {
    NOTREACHED();
    return;
  }
  if (state)
    gtk_tree_selection_select_iter(tree_selection_, &iter);
  else
    gtk_tree_selection_unselect_iter(tree_selection_, &iter);
}

void NativeTableGtk::SetFocusState(int model_row, bool state) {
  NOTIMPLEMENTED();
}

bool NativeTableGtk::IsRowSelected(int model_row) const {
  GtkTreeIter iter;
  if (!gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(gtk_model_), &iter, NULL,
                                     model_row)) {
    NOTREACHED();
    return false;
  }
  return gtk_tree_selection_iter_is_selected(tree_selection_, &iter);
}

void NativeTableGtk::OnRowsChanged(int start, int length) {
  GtkTreeIter iter;
  if (!gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(gtk_model_), &iter, NULL,
                                     start)) {
    NOTREACHED();
    return;
  }
  for (int i = start; i < start + length; i++) {
    GtkTreePath* tree_path =
        gtk_tree_model_get_path(GTK_TREE_MODEL(gtk_model_), &iter);
    gtk_tree_model_row_changed(GTK_TREE_MODEL(gtk_model_), tree_path, &iter);
    gtk_tree_path_free(tree_path);
    SetRowData(i, &iter);
    gboolean r = gtk_tree_model_iter_next(GTK_TREE_MODEL(gtk_model_), &iter);
    DCHECK(r || i == start + length - 1);  // (start + length - 1) might be the
                                           // last item, in which case we won't
                                           // get a next iterator.
  }
}

void NativeTableGtk::OnRowsAdded(int start, int length) {
  GtkTreeIter iter;
  gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(gtk_model_), &iter,
                                NULL, start);
  for (int i = start; i < start + length; i++) {
    gtk_list_store_append(gtk_model_, &iter);
    SetRowData(i, &iter);
  }
}

void NativeTableGtk::OnRowsRemoved(int start, int length) {
  GtkTreeIter iter;
  gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(gtk_model_), &iter,
                                NULL, start);
  for (int i = start; i < start + length; i++) {
    gboolean r = gtk_list_store_remove(gtk_model_, &iter);
    DCHECK(r || i == start + length - 1);  // (start + length - 1) might be the
                                           // last item, in which case we won't
                                           // get a next iterator.
  }
}

gfx::Rect NativeTableGtk::GetBounds() const {
  NOTIMPLEMENTED();
  return gfx::Rect();
}

void NativeTableGtk::CreateNativeControl() {
  if (table_->type() == CHECK_BOX_AND_TEXT) {
    // We are not supporting checkbox in tables on Gtk yet, as it is not used
    // in Chrome at this point in time
    NOTREACHED();
  }

  tree_view_ = GTK_TREE_VIEW(gtk_tree_view_new());
  // The tree view must be wrapped in a scroll-view to be scrollable.
  GtkWidget* scrolled = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled),
                                      GTK_SHADOW_ETCHED_IN);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_container_add(GTK_CONTAINER(scrolled), GTK_WIDGET(tree_view_));
  NativeControlCreated(scrolled);
  // native_view() is now available.

  // Set the selection mode, single or multiple.
  tree_selection_ = gtk_tree_view_get_selection(tree_view_);
  gtk_tree_selection_set_mode(
      tree_selection_, table_->single_selection() ? GTK_SELECTION_SINGLE :
                                                    GTK_SELECTION_MULTIPLE);

  // Don't make the header clickable until we support sorting.
  gtk_tree_view_set_headers_clickable(tree_view_, FALSE);

  // Show horizontal separator lines only.
  gtk_tree_view_set_grid_lines(tree_view_, GTK_TREE_VIEW_GRID_LINES_HORIZONTAL);

  int gtk_column_index = 0;
  size_t column_index = 0;
  if (table_->type() == ICON_AND_TEXT) {
    InsertIconAndTextColumn(table_->GetVisibleColumnAt(0), 0);
    column_index = 1;
    gtk_column_index = 2;
  }

  for (; column_index < table_->GetVisibleColumnCount();
       ++column_index, gtk_column_index++) {
    InsertTextColumn(table_->GetVisibleColumnAt(column_index),
                     gtk_column_index);
  }

  // Now create the model.
  int column_count = table_->GetVisibleColumnCount();
  scoped_array<GType> types(
      new GType[column_count + 1]);  // One extra column for the icon (if any).
  for (int i = 0; i < column_count + 1; i++)
    types[i] = G_TYPE_STRING;

  if (table_->type() == ICON_AND_TEXT) {
    types[0] = GDK_TYPE_PIXBUF;
    gtk_model_ = gtk_list_store_newv(column_count + 1, types.get());
  } else {
    gtk_model_ = gtk_list_store_newv(column_count, types.get());
  }

  gtk_tree_view_set_model(tree_view_, GTK_TREE_MODEL(gtk_model_));
  g_object_unref(gtk_model_);  // Now the tree owns the model.

  // Updates the gtk model with the actual model.
  if (table_->model())
    OnRowsAdded(0, table_->model()->RowCount());
}

void NativeTableGtk::InsertTextColumn(const TableColumn& column, int index) {
  GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_insert_column_with_attributes(tree_view_, -1,
                                              WideToUTF8(column.title).c_str(),
                                              renderer, "text", index, NULL);
}

void NativeTableGtk::InsertIconAndTextColumn(const TableColumn& column,
                                             int index) {
  // If necessary we could support more than 1 icon and text column and we could
  // make it so it does not have to be the 1st column.
  DCHECK(index == 0) << "The icon and text column can only be the first column "
      "at this point.";

  GtkTreeViewColumn* gtk_column = gtk_tree_view_column_new();
  gtk_tree_view_column_set_title(gtk_column, WideToUTF8(column.title).c_str());
  GtkCellRenderer* renderer = gtk_cell_renderer_pixbuf_new();
  gtk_tree_view_column_pack_start(gtk_column, renderer, FALSE);
  // First we set the icon renderer at index 0.
  gtk_tree_view_column_set_attributes(gtk_column, renderer, "pixbuf", 0, NULL);

  renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_column_pack_start(gtk_column, renderer, TRUE);
  // Then we set the text renderer at index 1.
  gtk_tree_view_column_set_attributes(gtk_column, renderer, "text", 1, NULL);

  gtk_tree_view_append_column(tree_view_, gtk_column);
}

void NativeTableGtk::SetRowData(int row_index, GtkTreeIter* iter) {
  int gtk_column_index = 0;
  if (table_->type() == ICON_AND_TEXT) {
    GdkPixbuf* icon = GetModelIcon(row_index);
    gtk_list_store_set(gtk_model_, iter, 0, icon, -1);
    g_object_unref(icon);
    gtk_column_index++;
  }
  for (size_t i = 0; i < table_->GetVisibleColumnCount();
       ++i, ++gtk_column_index) {
    std::string text =
        WideToUTF8(table_->model()->GetText(row_index,
                                            table_->GetVisibleColumnAt(i).id));
    gtk_list_store_set(gtk_model_, iter, gtk_column_index, text.c_str(), -1);
  }
}

GdkPixbuf* NativeTableGtk::GetModelIcon(int row) {
  SkBitmap icon = table_->model()->GetIcon(row);
  return gfx::GdkPixbufFromSkBitmap(&icon);
}

// static
NativeTableWrapper* NativeTableWrapper::CreateNativeWrapper(TableView2* table) {
  return new NativeTableGtk(table);
}

}  // namespace views
