// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/table/table_view_views.h"

#include "base/i18n/rtl.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/models/table_model.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/native_theme.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/border.h"
#include "ui/views/controls/scroll_view.h"
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


TableView::TableView(ui::TableModel* model,
                     const std::vector<ui::TableColumn>& columns,
                     TableTypes table_type,
                     bool single_selection,
                     bool resizable_columns,
                     bool autosize_columns)
    : model_(NULL),
      table_type_(table_type),
      table_view_observer_(NULL),
      selected_row_(-1),
      row_height_(font_.GetHeight() + kTextVerticalPadding * 2) {
  // This implementation only shows a single column.
  DCHECK_EQ(1u, columns.size());
  // CHECK_BOX_AND_TEXT is not supported.
  DCHECK(table_type == TEXT_ONLY || table_type == ICON_AND_TEXT);
  set_focusable(true);
  set_background(Background::CreateSolidBackground(SK_ColorWHITE));
  SetModel(model);
}

TableView::~TableView() {
  if (model_)
    model_->SetObserver(NULL);
}

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
  ScrollView* scroll_view = new ScrollView;
  scroll_view->SetContents(this);
  scroll_view->set_border(Border::CreateSolidBorder(
      1, gfx::NativeTheme::instance()->GetSystemColor(
          gfx::NativeTheme::kColorId_UnfocusedBorderColor)));
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
  if (selected_row_ != -1)
    ScrollRectToVisible(GetRowBounds(selected_row_));
  SchedulePaint();
  if (table_view_observer_)
    table_view_observer_->OnSelectionChanged();
}

int TableView::FirstSelectedRow() {
  return selected_row_;
}

void TableView::Layout() {
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
  return gfx::Size(50, RowCount() * row_height_);
}

bool TableView::OnKeyPressed(const KeyEvent& event) {
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

bool TableView::OnMousePressed(const MouseEvent& event) {
  RequestFocus();
  int row = event.y() / row_height_;
  if (row >= 0 && row < RowCount()) {
    Select(row);
    if (table_view_observer_ && event.flags() & ui::EF_IS_DOUBLE_CLICK)
      table_view_observer_->OnDoubleClick();
  }
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

  if (!RowCount())
    return;

  int min_y, max_y;
  {
    SkRect sk_clip_rect;
    if (canvas->sk_canvas()->getClipBounds(&sk_clip_rect)) {
      gfx::Rect clip_rect = gfx::SkRectToRect(sk_clip_rect);
      min_y = clip_rect.y();
      max_y = clip_rect.bottom();
    } else {
      gfx::Rect vis_bounds = GetVisibleBounds();
      min_y = vis_bounds.y();
      max_y = vis_bounds.bottom();
    }
  }

  int min_row = std::min(RowCount() - 1, std::max(0, min_y / row_height_));
  int max_row = max_y / row_height_;
  if (max_y % row_height_ != 0)
    max_row++;
  max_row = std::min(max_row, RowCount());
  for (int i = min_row; i < max_row; ++i) {
    gfx::Rect row_bounds(GetRowBounds(i));
    if (i == selected_row_) {
      canvas->FillRect(row_bounds, kSelectedBackgroundColor);
      if (HasFocus())
        canvas->DrawFocusRect(row_bounds);
    }
    int text_x = kTextHorizontalPadding;
    if (table_type_ == ICON_AND_TEXT) {
      SkBitmap image = model_->GetIcon(i);
      if (!image.isNull()) {
        int image_x = GetMirroredXWithWidthInView(text_x, image.width());
        canvas->DrawBitmapInt(
            image, 0, 0, image.width(), image.height(),
            image_x, row_bounds.y() + (row_bounds.height() - kImageSize) / 2,
            kImageSize, kImageSize, true);
      }
      text_x += kImageSize + kImageToTextPadding;
    }
    canvas->DrawStringInt(
        model_->GetText(i, 0), font_, kTextColor,
        GetMirroredXWithWidthInView(text_x, row_bounds.width() - text_x),
        row_bounds.y() + kTextVerticalPadding,
        row_bounds.width() - text_x,
        row_bounds.height() - kTextVerticalPadding * 2);
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

}  // namespace views
