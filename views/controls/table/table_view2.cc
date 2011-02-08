
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/table/table_view2.h"

#include "base/logging.h"
#include "ui/base/models/table_model.h"
#include "views/controls/native/native_view_host.h"
#include "views/controls/table/table_view_observer.h"

using ui::TableColumn;

namespace views {

// TableView2 ------------------------------------------------------------------

TableView2::TableView2(TableModel* model,
                       const std::vector<TableColumn>& columns,
                       TableTypes table_type,
                       int options)
    : model_(model),
      table_type_(table_type),
      table_view_observer_(NULL),
      visible_columns_(),
      all_columns_(),
      column_count_(static_cast<int>(columns.size())),
      single_selection_((options & SINGLE_SELECTION) != 0),
      resizable_columns_((options & RESIZABLE_COLUMNS) != 0),
      autosize_columns_((options & AUTOSIZE_COLUMNS) != 0),
      horizontal_lines_((options & HORIZONTAL_LINES) != 0),
      vertical_lines_((options & VERTICAL_LINES) != 0),
      native_wrapper_(NULL) {
  Init(columns);
}

TableView2::~TableView2() {
  if (model_)
    model_->SetObserver(NULL);
}

void TableView2::SetModel(TableModel* model) {
  if (model == model_)
    return;

  if (model_)
    model_->SetObserver(NULL);
  model_ = model;
  if (model_)
    model_->SetObserver(this);
  if (native_wrapper_)
    OnModelChanged();
}

int TableView2::GetRowCount() {
  if (!native_wrapper_)
    return 0;
  return native_wrapper_->GetRowCount();
}

int TableView2::SelectedRowCount() {
  if (!native_wrapper_)
    return 0;
  return native_wrapper_->GetSelectedRowCount();
}

void TableView2::ClearSelection() {
  if (native_wrapper_)
    native_wrapper_->ClearSelection();
}

void TableView2::ClearRowFocus() {
  if (native_wrapper_)
    native_wrapper_->ClearRowFocus();
}

int TableView2::GetFirstSelectedRow() {
  if (!native_wrapper_)
    return -1;
  return native_wrapper_->GetFirstSelectedRow();
}

int TableView2::GetFirstFocusedRow() {
  if (!native_wrapper_)
    return -1;
  return native_wrapper_->GetFirstFocusedRow();
}

void TableView2::SelectRow(int model_row) {
  if (!native_wrapper_)
    return;

  native_wrapper_->ClearSelection();
  native_wrapper_->SetSelectedState(model_row, true);
  if (table_view_observer_)
    table_view_observer_->OnSelectionChanged();
}

void TableView2::FocusRow(int model_row) {
 if (!native_wrapper_)
    return;

  native_wrapper_->SetFocusState(model_row, true);
}

bool TableView2::IsRowSelected(int model_row) {
  if (!native_wrapper_)
    return false;

  return native_wrapper_->IsRowSelected(model_row);
}

bool TableView2::IsRowFocused(int model_row) {
  if (!native_wrapper_)
    return false;

  return native_wrapper_->IsRowFocused(model_row);
}

void TableView2::OnModelChanged() {
  if (!native_wrapper_)
    return;

  int current_row_count = native_wrapper_->GetRowCount();
  if (current_row_count > 0)
    OnItemsRemoved(0, current_row_count);
  if (model_ && model_->RowCount())
    OnItemsAdded(0, model_->RowCount());
}

void TableView2::OnItemsChanged(int start, int length) {
  if (!native_wrapper_)
    return;

  if (length == -1) {
    DCHECK_GE(start, 0);
    length = model_->RowCount() - start;
  }
  native_wrapper_->OnRowsChanged(start, length);
}

void TableView2::OnItemsAdded(int start, int length) {
  if (!native_wrapper_)
    return;

  DCHECK(start >= 0 && length >= 0 && start <= native_wrapper_->GetRowCount());

  native_wrapper_->OnRowsAdded(start, length);
}

void TableView2::OnItemsRemoved(int start, int length) {
  if (!native_wrapper_)
    return;

  DCHECK(start >= 0 && length >= 0 &&
         start + length <= native_wrapper_->GetRowCount());

  native_wrapper_->OnRowsRemoved(start, length);
}

void TableView2::AddColumn(const TableColumn& col) {
  DCHECK_EQ(0U, all_columns_.count(col.id));
  all_columns_[col.id] = col;
}

void TableView2::SetColumns(const std::vector<TableColumn>& columns) {
  // Remove the currently visible columns.
  while (!visible_columns_.empty())
    SetColumnVisibility(visible_columns_.front(), false);

  all_columns_.clear();
  for (std::vector<TableColumn>::const_iterator i = columns.begin();
       i != columns.end(); ++i) {
    AddColumn(*i);
  }
}

void TableView2::OnColumnsChanged() {
  column_count_ = static_cast<int>(visible_columns_.size());
  ResetColumnSizes();
}

bool TableView2::HasColumn(int id) {
  return all_columns_.count(id) > 0;
}

void TableView2::SetColumnVisibility(int id, bool is_visible) {
  bool changed = false;
  for (std::vector<int>::iterator i = visible_columns_.begin();
       i != visible_columns_.end(); ++i) {
    if (*i == id) {
      if (is_visible) {
        // It's already visible, bail out early.
        return;
      } else {
        int index = static_cast<int>(i - visible_columns_.begin());
        // This could be called before the native list view has been created
        // (in CreateNativeControl, called when the view is added to a
        // Widget). In that case since the column is not in
        // visible_columns_ it will not be added later on when it is created.
        if (native_wrapper_)
          native_wrapper_->RemoveColumn(index);
        visible_columns_.erase(i);
        changed = true;
        break;
      }
    }
  }
  if (is_visible) {
    DCHECK(native_wrapper_);
    visible_columns_.push_back(id);
    TableColumn& column = all_columns_[id];
    native_wrapper_->InsertColumn(column, column_count_);
    changed = true;
  }
  if (changed)
    OnColumnsChanged();

}

bool TableView2::IsColumnVisible(int id) const {
  for (std::vector<int>::const_iterator i = visible_columns_.begin();
       i != visible_columns_.end(); ++i)
    if (*i == id) {
      return true;
    }
  return false;
}

void TableView2::ResetColumnSizes() {
  if (!native_wrapper_)
    return;

  // See comment in TableColumn for what this does.
  int width = this->width();
  gfx::Rect native_bounds = native_wrapper_->GetBounds();
  if (!native_bounds.IsEmpty()) {
    if (native_bounds.width() > 0) {
      // Prefer the bounds of the window over our bounds, which may be
      // different.
      width = native_bounds.width();
    }
  }

  float percent = 0;
  int fixed_width = 0;
  int autosize_width = 0;

  for (std::vector<int>::const_iterator i = visible_columns_.begin();
       i != visible_columns_.end(); ++i) {
    TableColumn& col = all_columns_[*i];
    int col_index = static_cast<int>(i - visible_columns_.begin());
    if (col.width == -1) {
      if (col.percent > 0) {
        percent += col.percent;
      } else {
        autosize_width += col.min_visible_width;
      }
    } else {
      fixed_width += native_wrapper_->GetColumnWidth(col_index);
    }
  }

  // Now do a pass to set the actual sizes of auto-sized and
  // percent-sized columns.
  int available_width = width - fixed_width - autosize_width;
  for (std::vector<int>::const_iterator i = visible_columns_.begin();
       i != visible_columns_.end(); ++i) {
    TableColumn& col = all_columns_[*i];
    if (col.width == -1) {
      int col_index = static_cast<int>(i - visible_columns_.begin());
      if (col.percent > 0) {
        if (available_width > 0) {
          int col_width =
            static_cast<int>(available_width * (col.percent / percent));
          available_width -= col_width;
          percent -= col.percent;
          native_wrapper_->SetColumnWidth(col_index, col_width);
        }
      } else {
        int col_width = col.min_visible_width;
        // If no "percent" columns, the last column acts as one, if auto-sized.
        if (percent == 0.f && available_width > 0 &&
            col_index == column_count_ - 1) {
          col_width += available_width;
        }
        native_wrapper_->SetColumnWidth(col_index, col_width);
      }
    }
  }
}

void TableView2::Layout() {
  if (native_wrapper_) {
    native_wrapper_->GetView()->SetBounds(0, 0, width(), height());
    native_wrapper_->GetView()->Layout();
  }
}

void TableView2::PaintFocusBorder(gfx::Canvas* canvas) {
  if (NativeViewHost::kRenderNativeControlFocus)
    View::PaintFocusBorder(canvas);
}

size_t TableView2::GetVisibleColumnCount() {
  return visible_columns_.size();
}

void TableView2::ViewHierarchyChanged(bool is_add, View* parent, View* child) {
  if (is_add && !native_wrapper_ && GetWidget()) {
    // The native wrapper's lifetime will be managed by the view hierarchy after
    // we call AddChildView.
    native_wrapper_ = CreateWrapper();
    AddChildView(native_wrapper_->GetView());
  }
}

void TableView2::Init(const std::vector<TableColumn>& columns) {
  for (std::vector<TableColumn>::const_iterator i = columns.begin();
      i != columns.end(); ++i) {
    AddColumn(*i);
    visible_columns_.push_back(i->id);
  }
}

gfx::NativeView TableView2::GetTestingHandle() {
  return native_wrapper_->GetTestingHandle();
}

TableColumn TableView2::GetVisibleColumnAt(int index) {
  DCHECK(index < static_cast<int>(visible_columns_.size()));
  std::map<int, TableColumn>::iterator iter =
      all_columns_.find(index);
  DCHECK(iter != all_columns_.end());
  return iter->second;
}

////////////////////////////////////////////////////////////////////////////////
// NativeTable2, protected:

NativeTableWrapper* TableView2::CreateWrapper() {
  return NativeTableWrapper::CreateNativeWrapper(this);
}

}  // namespace views
