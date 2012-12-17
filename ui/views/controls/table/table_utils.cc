// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/table/table_utils.h"

#include "base/logging.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"

namespace views {

const int kUnspecifiedColumnWidth = 90;

int WidthForContent(const gfx::Font& header_font,
                    const gfx::Font& content_font,
                    int padding,
                    const ui::TableColumn& column,
                    ui::TableModel* model) {
  int width = 0;
  if (!column.title.empty())
    width = header_font.GetStringWidth(column.title);

  for (int i = 0, row_count = model->RowCount(); i < row_count; ++i) {
    const int cell_width =
        content_font.GetStringWidth(model->GetText(i, column.id));
    width = std::max(width, cell_width);
  }
  return width + padding;
}

std::vector<int> CalculateTableColumnSizes(
    int width,
    const gfx::Font& header_font,
    const gfx::Font& content_font,
    int padding,
    const std::vector<ui::TableColumn>& columns,
    ui::TableModel* model) {
  float total_percent = 0;
  int non_percent_width = 0;
  std::vector<int> content_widths(columns.size(), 0);
  for (size_t i = 0; i < columns.size(); ++i) {
    const ui::TableColumn& column(columns[i]);
    if (column.width <= 0) {
      if (column.percent > 0) {
        total_percent += column.percent;
      } else {
        content_widths[i] = WidthForContent(header_font, content_font, padding,
                                            column, model);
        non_percent_width += content_widths[i];
      }
    } else {
      content_widths[i] = column.width;
      non_percent_width += column.width;
    }
  }

  std::vector<int> widths;
  const int available_width = width - non_percent_width;
  for (size_t i = 0; i < columns.size(); ++i) {
    const ui::TableColumn& column = columns[i];
    int column_width = content_widths[i];
    if (column.width <= 0 && column.percent > 0 && available_width > 0) {
      column_width = static_cast<int>(available_width *
                                      (column.percent / total_percent));
    }
    widths.push_back(column_width == 0 ? kUnspecifiedColumnWidth :
                     column_width);
  }

  // If no columns have specified a percent give the last column all the extra
  // space.
  if (!columns.empty() && total_percent == 0.f && available_width > 0 &&
      columns.back().width <= 0 && columns.back().percent == 0.f) {
    widths.back() += available_width;
  }

  return widths;
}

int TableColumnAlignmentToCanvasAlignment(
    ui::TableColumn::Alignment alignment) {
  switch (alignment) {
    case ui::TableColumn::LEFT:
      return gfx::Canvas::TEXT_ALIGN_LEFT;
    case ui::TableColumn::CENTER:
      return gfx::Canvas::TEXT_ALIGN_CENTER;
    case ui::TableColumn::RIGHT:
      return gfx::Canvas::TEXT_ALIGN_RIGHT;
  }
  NOTREACHED();
  return gfx::Canvas::TEXT_ALIGN_LEFT;
}

}  // namespace views
