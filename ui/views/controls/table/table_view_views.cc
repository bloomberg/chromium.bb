// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/table/table_view_views.h"

#include "base/i18n/rtl.h"
#include "ui/base/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/table/table_header.h"
#include "ui/views/controls/table/table_utils.h"
#include "ui/views/controls/table/table_view_observer.h"

// Padding around the text (on each side).
static const int kTextVerticalPadding = 3;
static const int kTextHorizontalPadding = 2;

// TODO: these should come from native theme or something.
static const SkColor kSelectedBackgroundColor = SkColorSetRGB(0xEE, 0xEE, 0xEE);
static const SkColor kTextColor = SK_ColorBLACK;

// Size of images.
static const int kImageSize = 16;

// Padding between the image and text.
static const int kImageToTextPadding = 4;

namespace views {

TableView::VisibleColumn::VisibleColumn() : x(0), width(0) {}

TableView::VisibleColumn::~VisibleColumn() {}

TableView::PaintRegion::PaintRegion()
    : min_row(0),
      max_row(0),
      min_column(0),
      max_column(0) {
}

TableView::PaintRegion::~PaintRegion() {}

TableView::TableView(ui::TableModel* model,
                     const std::vector<ui::TableColumn>& columns,
                     TableTypes table_type,
                     bool single_selection,
                     bool resizable_columns,
                     bool autosize_columns)
    : model_(NULL),
      columns_(columns),
      header_(NULL),
      table_type_(table_type),
      table_view_observer_(NULL),
      selected_row_(-1),
      row_height_(font_.GetHeight() + kTextVerticalPadding * 2),
      last_parent_width_(0) {
  for (size_t i = 0; i < columns.size(); ++i) {
    VisibleColumn visible_column;
    visible_column.column = columns[i];
    visible_columns_.push_back(visible_column);
  }
  set_focusable(true);
  set_background(Background::CreateSolidBackground(SK_ColorWHITE));
  SetModel(model);
}

TableView::~TableView() {
  if (model_)
    model_->SetObserver(NULL);
}

// TODO: this doesn't support arbitrarily changing the model, rename this to
// ClearModel() or something.
void TableView::SetModel(ui::TableModel* model) {
  if (model == model_)
    return;

  if (model_)
    model_->SetObserver(NULL);
  model_ = model;
  if (RowCount())
    selected_row_ = 0;
  if (model_)
    model_->SetObserver(this);
}

View* TableView::CreateParentIfNecessary() {
  ScrollView* scroll_view = ScrollView::CreateScrollViewWithBorder();
  scroll_view->SetContents(this);
  CreateHeaderIfNecessary();
  if (header_)
    scroll_view->SetHeader(header_);
  return scroll_view;
}

int TableView::RowCount() const {
  return model_ ? model_->RowCount() : 0;
}

int TableView::SelectedRowCount() {
  return selected_row_ != -1 ? 1 : 0;
}

void TableView::Select(int model_row) {
  if (!model_)
    return;

  if (model_row == selected_row_)
    return;

  DCHECK(model_row >= 0 && model_row < RowCount());
  selected_row_ = model_row;
  if (selected_row_ != -1) {
    gfx::Rect vis_rect(GetVisibleBounds());
    const gfx::Rect row_bounds(GetRowBounds(selected_row_));
    vis_rect.set_y(row_bounds.y());
    vis_rect.set_height(row_bounds.height());
    ScrollRectToVisible(vis_rect);
  }
  SchedulePaint();
  if (table_view_observer_)
    table_view_observer_->OnSelectionChanged();
}

int TableView::FirstSelectedRow() {
  return selected_row_;
}

void TableView::Layout() {
  if (parent() && parent()->width() != last_parent_width_) {
    last_parent_width_ = parent()->width();
    UpdateVisibleColumnSizes();
  }
  // We have to override Layout like this since we're contained in a ScrollView.
  gfx::Size pref = GetPreferredSize();
  int width = pref.width();
  int height = pref.height();
  if (parent()) {
    width = std::max(parent()->width(), width);
    height = std::max(parent()->height(), height);
  }
  SetBounds(x(), y(), width, height);
}

gfx::Size TableView::GetPreferredSize() {
  int width = 50;
  if (header_ && !visible_columns_.empty())
    width = visible_columns_.back().x + visible_columns_.back().width;
  return gfx::Size(width, RowCount() * row_height_);
}

bool TableView::OnKeyPressed(const ui::KeyEvent& event) {
  if (!HasFocus())
    return false;

  switch (event.key_code()) {
    case ui::VKEY_UP:
      if (selected_row_ > 0)
        Select(selected_row_ - 1);
      else if (selected_row_ == -1 && RowCount())
        Select(RowCount() - 1);
      return true;

    case ui::VKEY_DOWN:
      if (selected_row_ == -1) {
        if (RowCount())
          Select(0);
      } else if (selected_row_ + 1 < RowCount()) {
        Select(selected_row_ + 1);
      }
      return true;

    default:
      break;
  }
  return false;
}

bool TableView::OnMousePressed(const ui::MouseEvent& event) {
  RequestFocus();
  int row = event.y() / row_height_;
  if (row >= 0 && row < RowCount()) {
    Select(row);
    if (table_view_observer_ && event.flags() & ui::EF_IS_DOUBLE_CLICK)
      table_view_observer_->OnDoubleClick();
  }
  return true;
}

bool TableView::OnMouseDragged(const ui::MouseEvent& event) {
  int row = event.y() / row_height_;
  if (row >= 0 && row < RowCount())
    Select(row);
  return true;
}

void TableView::OnModelChanged() {
  if (RowCount())
    selected_row_ = 0;
  else
    selected_row_ = -1;
  NumRowsChanged();
}

void TableView::OnItemsChanged(int start, int length) {
  SchedulePaint();
}

void TableView::OnItemsAdded(int start, int length) {
  if (selected_row_ >= start)
    selected_row_ += length;
  NumRowsChanged();
}

void TableView::OnItemsRemoved(int start, int length) {
  bool notify_selection_changed = false;
  if (selected_row_ >= (start + length)) {
    selected_row_ -= length;
    if (selected_row_ == 0 && RowCount() == 0) {
      selected_row_ = -1;
      notify_selection_changed = true;
    }
  } else if (selected_row_ >= start) {
    selected_row_ = start;
    if (selected_row_ == RowCount())
      selected_row_--;
    notify_selection_changed = true;
  }
  if (table_view_observer_ && notify_selection_changed)
    table_view_observer_->OnSelectionChanged();
}

gfx::Point TableView::GetKeyboardContextMenuLocation() {
  int first_selected = FirstSelectedRow();
  gfx::Rect vis_bounds(GetVisibleBounds());
  int y = vis_bounds.height() / 2;
  if (first_selected != -1) {
    gfx::Rect cell_bounds(GetRowBounds(first_selected));
    if (cell_bounds.bottom() >= vis_bounds.y() &&
        cell_bounds.bottom() < vis_bounds.bottom()) {
      y = cell_bounds.bottom();
    }
  }
  gfx::Point screen_loc(0, y);
  if (base::i18n::IsRTL())
    screen_loc.set_x(width());
  ConvertPointToScreen(this, &screen_loc);
  return screen_loc;
}

void TableView::OnPaint(gfx::Canvas* canvas) {
  // Don't invoke View::OnPaint so that we can render our own focus border.
  OnPaintBackground(canvas);

  if (!RowCount() || visible_columns_.empty())
    return;

  const PaintRegion region(GetPaintRegion(canvas));
  if (region.min_column == -1)
    return;  // No need to paint anything.

  const int icon_index = GetIconIndex();
  for (int i = region.min_row; i < region.max_row; ++i) {
    if (i == selected_row_) {
      const gfx::Rect row_bounds(GetRowBounds(i));
      canvas->FillRect(row_bounds, kSelectedBackgroundColor);
      if (HasFocus() && !header_)
        canvas->DrawFocusRect(row_bounds);
    }
    for (int j = region.min_column; j < region.max_column; ++j) {
      const gfx::Rect cell_bounds(GetCellBounds(i, j));
      int text_x = kTextHorizontalPadding + cell_bounds.x();
      if (j == icon_index) {
        gfx::ImageSkia image = model_->GetIcon(i);
        if (!image.isNull()) {
          int image_x = GetMirroredXWithWidthInView(text_x, image.width());
          canvas->DrawImageInt(
              image, 0, 0, image.width(), image.height(),
              image_x,
              cell_bounds.y() + (cell_bounds.height() - kImageSize) / 2,
              kImageSize, kImageSize, true);
        }
        text_x += kImageSize + kImageToTextPadding;
      }
      canvas->DrawStringInt(
          model_->GetText(i, visible_columns_[j].column.id), font_, kTextColor,
          GetMirroredXWithWidthInView(text_x, cell_bounds.right() - text_x),
          cell_bounds.y() + kTextVerticalPadding,
          cell_bounds.right() - text_x,
          cell_bounds.height() - kTextVerticalPadding * 2,
          TableColumnAlignmentToCanvasAlignment(
              visible_columns_[j].column.alignment));
    }
  }
}

void TableView::OnFocus() {
  if (selected_row_ != -1)
    SchedulePaintInRect(GetRowBounds(selected_row_));
}

void TableView::OnBlur() {
  if (selected_row_ != -1)
    SchedulePaintInRect(GetRowBounds(selected_row_));
}

void TableView::NumRowsChanged() {
  PreferredSizeChanged();
  SchedulePaint();
}

gfx::Rect TableView::GetRowBounds(int row) {
  return gfx::Rect(0, row * row_height_, width(), row_height_);
}

gfx::Rect TableView::GetCellBounds(int row, int visible_column_index) {
  if (!header_)
    return GetRowBounds(row);
  const VisibleColumn& vis_col(visible_columns_[visible_column_index]);
  return gfx::Rect(vis_col.x, row * row_height_, vis_col.width, row_height_);
}

void TableView::CreateHeaderIfNecessary() {
  // Only create a header if there is more than one column or the title of the
  // only column is not empty.
  if (header_ || (columns_.size() == 1 && columns_[0].title.empty()))
    return;

  header_ = new TableHeader(this);
}

void TableView::UpdateVisibleColumnSizes() {
  if (!header_)
    return;

  std::vector<ui::TableColumn> columns;
  for (size_t i = 0; i < visible_columns_.size(); ++i)
    columns.push_back(visible_columns_[i].column);
  std::vector<int> sizes =
      views::CalculateTableColumnSizes(last_parent_width_, header_->font(),
                                       font_, 0,  // TODO: fix this
                                       columns, model_);
  DCHECK_EQ(visible_columns_.size(), sizes.size());
  int x = 0;
  for (size_t i = 0; i < visible_columns_.size(); ++i) {
    visible_columns_[i].x = x;
    visible_columns_[i].width = sizes[i];
    x += sizes[i];
  }
}

TableView::PaintRegion TableView::GetPaintRegion(gfx::Canvas* canvas) {
  DCHECK(!visible_columns_.empty());
  DCHECK(RowCount());

  gfx::Rect paint_rect;
  SkRect sk_clip_rect;
  if (canvas->sk_canvas()->getClipBounds(&sk_clip_rect))
    paint_rect = gfx::ToEnclosingRect(gfx::SkRectToRectF(sk_clip_rect));
  else
    paint_rect = GetVisibleBounds();

  PaintRegion region;
  region.min_row = std::min(RowCount() - 1,
                            std::max(0, paint_rect.y() / row_height_));
  region.max_row = paint_rect.bottom() / row_height_;
  if (paint_rect.bottom() % row_height_ != 0)
    region.max_row++;
  region.max_row = std::min(region.max_row, RowCount());

  if (!header_) {
    region.max_column = 1;
    return region;
  }

  region.min_column = -1;
  region.max_column = visible_columns_.size();
  for (size_t i = 0; i < visible_columns_.size(); ++i) {
    int max_x = visible_columns_[i].x + visible_columns_[i].width;
    if (region.min_column == -1 && max_x >= paint_rect.x())
      region.min_column = static_cast<int>(i);
    if (region.min_column != -1 &&
        visible_columns_[i].x >= paint_rect.right()) {
      region.max_column = i;
      break;
    }
  }
  return region;
}

int TableView::GetIconIndex() {
  if (table_type_ != ICON_AND_TEXT || columns_.empty())
    return -1;
  if (!header_)
    return 0;

  for (size_t i = 0; i < visible_columns_.size(); ++i) {
    if (visible_columns_[i].column.id == columns_[0].id)
      return static_cast<int>(i);
  }
  return -1;
}

}  // namespace views
