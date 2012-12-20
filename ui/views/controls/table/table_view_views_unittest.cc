// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// For WinDDK ATL compatibility, these ATL headers must come first.

#include "ui/views/controls/table/table_view_views.h"

#include "base/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/table/table_header.h"
#include "ui/views/controls/table/test_table_model.h"

// Put the tests in the views namespace to make it easier to declare them as
// friend classes.
namespace views {

class TableViewTestHelper {
 public:
  explicit TableViewTestHelper(TableView* table) : table_(table) {}

  std::string GetPaintRegion(const gfx::Rect& bounds) {
    TableView::PaintRegion region(table_->GetPaintRegion(bounds));
    return "rows=" + base::IntToString(region.min_row) + " " +
        base::IntToString(region.max_row) + " cols=" +
        base::IntToString(region.min_column) + " " +
        base::IntToString(region.max_column);
  }

  size_t visible_col_count() {
    return table_->visible_columns().size();
  }

  TableHeader* header() { return table_->header_; }

 private:
  TableView* table_;

  DISALLOW_COPY_AND_ASSIGN(TableViewTestHelper);
};

class TableViewTest : public testing::Test {
 public:
  TableViewTest() : table_(NULL) {}

  virtual void SetUp() OVERRIDE {
    model_.reset(new TestTableModel(4));
    std::vector<ui::TableColumn> columns(2);
    columns[1].id = 1;
    table_ =
        new TableView(model_.get(), columns, TEXT_ONLY, true, true, true);
    parent_.reset(table_->CreateParentIfNecessary());
    parent_->SetBounds(0, 0, 10000, 10000);
    parent_->Layout();
    helper_.reset(new TableViewTestHelper(table_));
  }

 protected:
  scoped_ptr<TestTableModel> model_;

  // Owned by |parent_|.
  TableView* table_;

  scoped_ptr<TableViewTestHelper> helper_;

 private:
  scoped_ptr<View> parent_;

  DISALLOW_COPY_AND_ASSIGN(TableViewTest);
};

// Verifies GetPaintRegion.
TEST_F(TableViewTest, GetPaintRegion) {
  // Two columns should be visible.
  EXPECT_EQ(2u, helper_->visible_col_count());

  EXPECT_EQ("rows=0 4 cols=0 2", helper_->GetPaintRegion(table_->bounds()));
  EXPECT_EQ("rows=0 4 cols=0 1",
            helper_->GetPaintRegion(gfx::Rect(0, 0, 1, table_->height())));
}

// Verifies SetColumnVisibility().
TEST_F(TableViewTest, ColumnVisibility) {
  // Two columns should be visible.
  EXPECT_EQ(2u, helper_->visible_col_count());

  // Should do nothing (column already visible).
  table_->SetColumnVisibility(0, true);
  EXPECT_EQ(2u, helper_->visible_col_count());

  // Hide the first column.
  table_->SetColumnVisibility(0, false);
  ASSERT_EQ(1u, helper_->visible_col_count());
  EXPECT_EQ(1, table_->visible_columns()[0].column.id);
  EXPECT_EQ("rows=0 4 cols=0 1", helper_->GetPaintRegion(table_->bounds()));

  // Hide the second column.
  table_->SetColumnVisibility(1, false);
  EXPECT_EQ(0u, helper_->visible_col_count());

  // Show the second column.
  table_->SetColumnVisibility(1, true);
  ASSERT_EQ(1u, helper_->visible_col_count());
  EXPECT_EQ(1, table_->visible_columns()[0].column.id);
  EXPECT_EQ("rows=0 4 cols=0 1", helper_->GetPaintRegion(table_->bounds()));

  // Show the first column.
  table_->SetColumnVisibility(0, true);
  ASSERT_EQ(2u, helper_->visible_col_count());
  EXPECT_EQ(1, table_->visible_columns()[0].column.id);
  EXPECT_EQ(0, table_->visible_columns()[1].column.id);
  EXPECT_EQ("rows=0 4 cols=0 2", helper_->GetPaintRegion(table_->bounds()));
}

// Verifies resizing a column works.
TEST_F(TableViewTest, Resize) {
  const int x = table_->visible_columns()[0].width;
  EXPECT_NE(0, x);
  // Drag the mouse 1 pixel to the left.
  const ui::MouseEvent pressed(ui::ET_MOUSE_PRESSED, gfx::Point(x, 0),
                               gfx::Point(x, 0), ui::EF_LEFT_MOUSE_BUTTON);
  helper_->header()->OnMousePressed(pressed);
  const ui::MouseEvent dragged(ui::ET_MOUSE_DRAGGED, gfx::Point(x - 1, 0),
                               gfx::Point(x - 1, 0), ui::EF_LEFT_MOUSE_BUTTON);
  helper_->header()->OnMouseDragged(dragged);

  // This should shrink the first column and pull the second column in.
  EXPECT_EQ(x - 1, table_->visible_columns()[0].width);
  EXPECT_EQ(x - 1, table_->visible_columns()[1].x);
}

}  // namespace views
