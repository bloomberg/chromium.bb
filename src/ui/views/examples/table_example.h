// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_TABLE_EXAMPLE_H_
#define UI_VIEWS_EXAMPLES_TABLE_EXAMPLE_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/models/table_model.h"
#include "ui/views/controls/table/table_grouper.h"
#include "ui/views/controls/table/table_view.h"
#include "ui/views/controls/table/table_view_observer.h"
#include "ui/views/examples/example_base.h"

namespace views {
class Checkbox;
class TableView;

namespace examples {

class VIEWS_EXAMPLES_EXPORT TableExample : public ExampleBase,
                                           public ui::TableModel,
                                           public TableGrouper,
                                           public TableViewObserver {
 public:
  TableExample();

  TableExample(const TableExample&) = delete;
  TableExample& operator=(const TableExample&) = delete;

  ~TableExample() override;

  // ExampleBase:
  void CreateExampleView(View* container) override;

  // ui::TableModel:
  int RowCount() override;
  std::u16string GetText(int row, int column_id) override;
  ui::ImageModel GetIcon(int row) override;
  std::u16string GetTooltip(int row) override;
  void SetObserver(ui::TableModelObserver* observer) override;

  // TableGrouper:
  void GetGroupRange(int model_index, GroupRange* range) override;

  // TableViewObserver:
  void OnSelectionChanged() override;
  void OnDoubleClick() override;
  void OnMiddleClick() override;
  void OnKeyDown(ui::KeyboardCode virtual_keycode) override;

 private:
  // The table to be tested.
  raw_ptr<TableView> table_ = nullptr;

  raw_ptr<Checkbox> column1_visible_checkbox_ = nullptr;
  raw_ptr<Checkbox> column2_visible_checkbox_ = nullptr;
  raw_ptr<Checkbox> column3_visible_checkbox_ = nullptr;
  raw_ptr<Checkbox> column4_visible_checkbox_ = nullptr;

  SkBitmap icon1_;
  SkBitmap icon2_;
};

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_TABLE_EXAMPLE_H_
