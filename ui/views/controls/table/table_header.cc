// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/table/table_header.h"

#include "ui/gfx/canvas.h"
#include "ui/views/controls/table/table_utils.h"
#include "ui/views/controls/table/table_view.h"

#if defined(USE_AURA)
#include "ui/base/cursor/cursor.h"
#endif

namespace views {

namespace {

const int kVerticalPadding = 6;

// The minimum width we allow a column to go down to.
const int kMinColumnWidth = 10;

// Distace from edge columns can be resized by.
const int kResizePadding = 5;

const SkColor kTextColor = SK_ColorBLACK;

gfx::NativeCursor GetResizeCursor() {
#if defined(USE_AURA)
  return ui::kCursorColumnResize;
#elif defined(OS_WIN)
  static HCURSOR g_hand_cursor = LoadCursor(NULL, IDC_SIZEWE);
  return g_hand_cursor;
#endif
}

}  // namespace

// static
const int TableHeader::kHorizontalPadding = 4;

typedef std::vector<TableView::VisibleColumn> Columns;

TableHeader::TableHeader(TableView* table) : table_(table) {
}

TableHeader::~TableHeader() {
}

void TableHeader::Layout() {
  SetBounds(x(), y(), table_->width(), GetPreferredSize().height());
}

void TableHeader::OnPaint(gfx::Canvas* canvas) {
  const Columns& columns = table_->visible_columns();
  for (size_t i = 0; i < columns.size(); ++i) {
    if (columns[i].column.title.empty())
      continue;

    const int x = columns[i].x + kHorizontalPadding;
    const int width =
        columns[i].width - kHorizontalPadding - kHorizontalPadding;
    if (width <= 0)
      continue;

    canvas->DrawStringInt(
        columns[i].column.title, font_, kTextColor,
        GetMirroredXWithWidthInView(x, width), kVerticalPadding, width,
        height() - kVerticalPadding * 2,
        TableColumnAlignmentToCanvasAlignment(columns[i].column.alignment));
  }
}

gfx::Size TableHeader::GetPreferredSize() {
  return gfx::Size(1, kVerticalPadding * 2 + font_.GetHeight());
}

gfx::NativeCursor TableHeader::GetCursor(const ui::MouseEvent& event) {
  return GetResizeColumn(event.x()) != -1 ? GetResizeCursor() :
      View::GetCursor(event);
}

bool TableHeader::OnMousePressed(const ui::MouseEvent& event) {
  if (event.IsOnlyLeftMouseButton()) {
    const int index = GetResizeColumn(event.x());
    if (index != -1) {
      DCHECK(!is_resizing());
      resize_details_.reset(new ColumnResizeDetails);
      resize_details_->column_index = index;
      resize_details_->initial_x = event.x();
      resize_details_->initial_width =
          table_->visible_columns()[index].width;
    }
    return true;
  }
  // Return false so that context menus on ancestors work.
  return false;
}

bool TableHeader::OnMouseDragged(const ui::MouseEvent& event) {
  if (is_resizing()) {
    const int delta = event.x() - resize_details_->initial_x;
    table_->SetVisibleColumnWidth(
        resize_details_->column_index,
        std::max(kMinColumnWidth, resize_details_->initial_width + delta));
  }
  return true;
}

void TableHeader::OnMouseReleased(const ui::MouseEvent& event) {
  const bool was_resizing = resize_details_ != NULL;
  resize_details_.reset();
  if (!was_resizing && event.IsOnlyLeftMouseButton() &&
      !table_->visible_columns().empty()) {
    const int index = GetClosestColumn(event.x());
    const TableView::VisibleColumn& column(table_->visible_columns()[index]);
    if (event.x() >= column.x && event.x() < column.x + column.width &&
        event.y() >= 0 && event.y() < height())
      table_->ToggleSortOrder(index);
  }
}

void TableHeader::OnMouseCaptureLost() {
  if (is_resizing()) {
    table_->SetVisibleColumnWidth(resize_details_->column_index,
                                  resize_details_->initial_width);
  }
  resize_details_.reset();
}

int TableHeader::GetClosestColumn(int x) const {
  const Columns& columns(table_->visible_columns());
  for (size_t i = 0; i < columns.size(); ++i) {
    if (x <= columns[i].x + columns[i].width)
      return static_cast<int>(i);
  }
  return static_cast<int>(columns.size()) - 1;
}

int TableHeader::GetResizeColumn(int x) const {
  const Columns& columns(table_->visible_columns());
  if (columns.empty())
    return -1;

  const int index = GetClosestColumn(x);
  DCHECK_NE(-1, index);
  const TableView::VisibleColumn& column(table_->visible_columns()[index]);
  if (index > 0 && x >= column.x - kResizePadding &&
      x <= column.x + kResizePadding) {
    return index - 1;
  }
  const int max_x = column.x + column.width;
  return (x >= max_x - kResizePadding && x <= max_x + kResizePadding) ?
      index : -1;
}

}  // namespace views
