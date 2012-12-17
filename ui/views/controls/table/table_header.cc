// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/table/table_header.h"

#include "ui/gfx/canvas.h"
#include "ui/views/controls/table/table_utils.h"
#include "ui/views/controls/table/table_view.h"

namespace views {

namespace {

const int kVerticalPadding = 6;
const int kHorizontalPadding = 4;

const SkColor kTextColor = SK_ColorBLACK;

}  // namespace

typedef std::vector<TableView::VisibleColumn> Columns;

TableHeader::TableHeader(TableView* table)
    : table_(table) {
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

}  // namespace views
