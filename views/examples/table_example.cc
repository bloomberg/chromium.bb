// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/examples/table_example.h"

#include <vector>

#include "third_party/skia/include/core/SkCanvas.h"
#include "views/controls/button/checkbox.h"
#include "views/layout/grid_layout.h"

namespace examples {

TableExample::TableExample(ExamplesMain* main) : ExampleBase(main) {}

TableExample::~TableExample() {}

std::wstring TableExample::GetExampleTitle() {
  return L"Table";
}

void TableExample::CreateExampleView(views::View* container) {
  column1_visible_checkbox_ = new views::Checkbox(L"Fruit column visible");
  column1_visible_checkbox_->SetChecked(true);
  column1_visible_checkbox_->set_listener(this);
  column2_visible_checkbox_ = new views::Checkbox(L"Color column visible");
  column2_visible_checkbox_->SetChecked(true);
  column2_visible_checkbox_->set_listener(this);
  column3_visible_checkbox_ = new views::Checkbox(L"Origin column visible");
  column3_visible_checkbox_->SetChecked(true);
  column3_visible_checkbox_->set_listener(this);
  column4_visible_checkbox_ = new views::Checkbox(L"Price column visible");
  column4_visible_checkbox_->SetChecked(true);
  column4_visible_checkbox_->set_listener(this);

  views::GridLayout* layout = new views::GridLayout(container);
  container->SetLayoutManager(layout);

  std::vector<ui::TableColumn> columns;
  columns.push_back(ui::TableColumn(0, L"Fruit", ui::TableColumn::LEFT, 100));
  columns.push_back(ui::TableColumn(1, L"Color", ui::TableColumn::LEFT, 100));
  columns.push_back(ui::TableColumn(2, L"Origin", ui::TableColumn::LEFT, 100));
  columns.push_back(ui::TableColumn(3, L"Price", ui::TableColumn::LEFT, 100));
  table_ = new views::TableView(this, columns, views::ICON_AND_TEXT,
                                true, true, true);
  table_->SetObserver(this);
  icon1_.setConfig(SkBitmap::kARGB_8888_Config, 16, 16);
  icon1_.allocPixels();
  SkCanvas canvas1(icon1_);
  canvas1.drawColor(SK_ColorRED);

  icon2_.setConfig(SkBitmap::kARGB_8888_Config, 16, 16);
  icon2_.allocPixels();
  SkCanvas canvas2(icon2_);
  canvas2.drawColor(SK_ColorBLUE);

  views::ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, 0);
  layout->StartRow(1 /* expand */, 0);
  layout->AddView(table_);

  column_set = layout->AddColumnSet(1);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                        0.5f, views::GridLayout::USE_PREF, 0, 0);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                        0.5f, views::GridLayout::USE_PREF, 0, 0);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                        0.5f, views::GridLayout::USE_PREF, 0, 0);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                        0.5f, views::GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0 /* no expand */, 1);

  layout->AddView(column1_visible_checkbox_);
  layout->AddView(column2_visible_checkbox_);
  layout->AddView(column3_visible_checkbox_);
  layout->AddView(column4_visible_checkbox_);
}

int TableExample::RowCount() {
  return 10;
}

std::wstring TableExample::GetText(int row, int column_id) {
  std::wstring cells[5][5] = {
    { L"Orange", L"Orange", L"South america", L"$5" },
    { L"Apple", L"Green", L"Canada", L"$3" },
    { L"Blue berries", L"Blue", L"Mexico", L"$10.3" },
    { L"Strawberries", L"Red", L"California", L"$7" },
    { L"Cantaloupe", L"Orange", L"South america", L"$5" },
  };
  return cells[row % 5][column_id];
}

SkBitmap TableExample::GetIcon(int row) {
  return row % 2 ? icon1_ : icon2_;
}

void TableExample::SetObserver(ui::TableModelObserver* observer) {}

void TableExample::OnSelectionChanged() {
  PrintStatus(L"Selection changed");
}

void TableExample::OnDoubleClick() {}

void TableExample::OnMiddleClick() {}

void TableExample::OnKeyDown(ui::KeyboardCode virtual_keycode) {}

void TableExample::OnTableViewDelete(views::TableView* table_view) {}

void TableExample::OnTableView2Delete(views::TableView2* table_view) {}

void TableExample::ButtonPressed(views::Button* sender,
                                 const views::Event& event) {
  int index = 0;
  bool show = true;
  if (sender == column1_visible_checkbox_) {
    index = 0;
    show = column1_visible_checkbox_->checked();
  } else if (sender == column2_visible_checkbox_) {
    index = 1;
    show = column2_visible_checkbox_->checked();
  } else if (sender == column3_visible_checkbox_) {
    index = 2;
    show = column3_visible_checkbox_->checked();
  } else if (sender == column4_visible_checkbox_) {
    index = 3;
    show = column4_visible_checkbox_->checked();
  }
  table_->SetColumnVisibility(index, show);
}

}  // namespace examples
