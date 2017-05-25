// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/grid_layout.h"

#include "base/compiler_specific.h"
#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/border.h"
#include "ui/views/test/platform_test_helper.h"
#include "ui/views/view.h"

namespace views {

void ExpectViewBoundsEquals(int x, int y, int w, int h,
                            const View* view) {
  EXPECT_EQ(x, view->x());
  EXPECT_EQ(y, view->y());
  EXPECT_EQ(w, view->width());
  EXPECT_EQ(h, view->height());
}

View* CreateSizedView(const gfx::Size& size) {
  auto* view = new View();
  view->SetPreferredSize(size);
  return view;
}

// A test view that wants to alter its preferred size and re-layout when it gets
// added to the View hierarchy.
class LayoutOnAddView : public View {
 public:
  LayoutOnAddView() { SetPreferredSize(gfx::Size(10, 10)); }

  void set_target_size(const gfx::Size& target_size) {
    target_size_ = target_size;
  }

  // View:
  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override {
    if (GetPreferredSize() == target_size_)
      return;

    // Contrive a realistic thing that a View might what to do, but which would
    // break the layout machinery. Note an override of OnNativeThemeChanged()
    // would be more compelling, but there is no Widget in this test harness.
    SetPreferredSize(target_size_);
    PreferredSizeChanged();
    parent()->Layout();
  }

 private:
  gfx::Size target_size_;

  DISALLOW_COPY_AND_ASSIGN(LayoutOnAddView);
};

// A view with fixed circumference that trades height for width.
class FlexibleView : public View {
 public:
  explicit FlexibleView(int circumference) {
    circumference_ = circumference;
  }

  gfx::Size CalculatePreferredSize() const override {
    return gfx::Size(0, circumference_ / 2);
  }

  int GetHeightForWidth(int width) const override {
    return std::max(0, circumference_ / 2 - width);
  }

 private:
   int circumference_;
};

class GridLayoutTest : public testing::Test {
 public:
  GridLayoutTest() : layout(&host) {}

  void RemoveAll() {
    for (int i = host.child_count() - 1; i >= 0; i--)
      host.RemoveChildView(host.child_at(i));
  }

  void GetPreferredSize() {
    pref = layout.GetPreferredSize(&host);
  }

  gfx::Size pref;
  gfx::Rect bounds;
  View host;
  GridLayout layout;
};

class GridLayoutAlignmentTest : public testing::Test {
 public:
  GridLayoutAlignmentTest() : layout(&host) {
    v1.SetPreferredSize(gfx::Size(10, 20));
  }

  void RemoveAll() {
    for (int i = host.child_count() - 1; i >= 0; i--)
      host.RemoveChildView(host.child_at(i));
  }

  void TestAlignment(GridLayout::Alignment alignment, gfx::Rect* bounds) {
    ColumnSet* c1 = layout.AddColumnSet(0);
    c1->AddColumn(alignment, alignment, 1, GridLayout::USE_PREF, 0, 0);
    layout.StartRow(1, 0);
    layout.AddView(&v1);
    gfx::Size pref = layout.GetPreferredSize(&host);
    EXPECT_EQ(gfx::Size(10, 20), pref);
    host.SetBounds(0, 0, 100, 100);
    layout.Layout(&host);
    *bounds = v1.bounds();
    RemoveAll();
  }

  View host;
  View v1;
  GridLayout layout;
};

TEST_F(GridLayoutAlignmentTest, Fill) {
  gfx::Rect bounds;
  TestAlignment(GridLayout::FILL, &bounds);
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100), bounds);
}

TEST_F(GridLayoutAlignmentTest, Leading) {
  gfx::Rect bounds;
  TestAlignment(GridLayout::LEADING, &bounds);
  EXPECT_EQ(gfx::Rect(0, 0, 10, 20), bounds);
}

TEST_F(GridLayoutAlignmentTest, Center) {
  gfx::Rect bounds;
  TestAlignment(GridLayout::CENTER, &bounds);
  EXPECT_EQ(gfx::Rect(45, 40, 10, 20), bounds);
}

TEST_F(GridLayoutAlignmentTest, Trailing) {
  gfx::Rect bounds;
  TestAlignment(GridLayout::TRAILING, &bounds);
  EXPECT_EQ(gfx::Rect(90, 80, 10, 20), bounds);
}

TEST_F(GridLayoutTest, TwoColumns) {
  View v1;
  v1.SetPreferredSize(gfx::Size(10, 20));
  View v2;
  v2.SetPreferredSize(gfx::Size(20, 20));
  ColumnSet* c1 = layout.AddColumnSet(0);
  c1->AddColumn(GridLayout::LEADING, GridLayout::LEADING,
                0, GridLayout::USE_PREF, 0, 0);
  c1->AddColumn(GridLayout::LEADING, GridLayout::LEADING,
                0, GridLayout::USE_PREF, 0, 0);
  layout.StartRow(0, 0);
  layout.AddView(&v1);
  layout.AddView(&v2);

  GetPreferredSize();
  EXPECT_EQ(gfx::Size(30, 20), pref);

  host.SetBounds(0, 0, pref.width(), pref.height());
  layout.Layout(&host);
  ExpectViewBoundsEquals(0, 0, 10, 20, &v1);
  ExpectViewBoundsEquals(10, 0, 20, 20, &v2);

  RemoveAll();
}

// Test linked column sizes, and the column size limit.
TEST_F(GridLayoutTest, LinkedSizes) {
  View v1;
  v1.SetPreferredSize(gfx::Size(10, 20));
  View v2;
  v2.SetPreferredSize(gfx::Size(20, 20));
  View v3;
  v3.SetPreferredSize(gfx::Size(0, 20));
  ColumnSet* c1 = layout.AddColumnSet(0);

  // Fill widths.
  c1->AddColumn(GridLayout::FILL, GridLayout::LEADING, 0, GridLayout::USE_PREF,
                0, 0);
  c1->AddColumn(GridLayout::FILL, GridLayout::LEADING, 0, GridLayout::USE_PREF,
                0, 0);
  c1->AddColumn(GridLayout::FILL, GridLayout::LEADING, 0, GridLayout::USE_PREF,
                0, 0);

  layout.StartRow(0, 0);
  layout.AddView(&v1);
  layout.AddView(&v2);
  layout.AddView(&v3);

  // Link all the columns.
  c1->LinkColumnSizes(0, 1, 2, -1);
  GetPreferredSize();

  // |v1| and |v3| should obtain the same width as |v2|, since |v2| is largest.
  EXPECT_EQ(gfx::Size(20 + 20 + 20, 20), pref);
  host.SetBounds(0, 0, pref.width(), pref.height());
  layout.Layout(&host);
  ExpectViewBoundsEquals(0, 0, 20, 20, &v1);
  ExpectViewBoundsEquals(20, 0, 20, 20, &v2);
  ExpectViewBoundsEquals(40, 0, 20, 20, &v3);

  // If the limit is zero, behaves as though the columns are not linked.
  c1->set_linked_column_size_limit(0);
  GetPreferredSize();
  EXPECT_EQ(gfx::Size(10 + 20 + 0, 20), pref);
  host.SetBounds(0, 0, pref.width(), pref.height());
  layout.Layout(&host);
  ExpectViewBoundsEquals(0, 0, 10, 20, &v1);
  ExpectViewBoundsEquals(10, 0, 20, 20, &v2);
  ExpectViewBoundsEquals(30, 0, 0, 20, &v3);

  // Set a size limit.
  c1->set_linked_column_size_limit(40);
  v1.SetPreferredSize(gfx::Size(35, 20));
  GetPreferredSize();

  // |v1| now dominates, but it is still below the limit.
  EXPECT_EQ(gfx::Size(35 + 35 + 35, 20), pref);
  host.SetBounds(0, 0, pref.width(), pref.height());
  layout.Layout(&host);
  ExpectViewBoundsEquals(0, 0, 35, 20, &v1);
  ExpectViewBoundsEquals(35, 0, 35, 20, &v2);
  ExpectViewBoundsEquals(70, 0, 35, 20, &v3);

  // Go over the limit. |v1| shouldn't influence size at all, but the others
  // should still be linked to the next largest width.
  v1.SetPreferredSize(gfx::Size(45, 20));
  GetPreferredSize();
  EXPECT_EQ(gfx::Size(45 + 20 + 20, 20), pref);
  host.SetBounds(0, 0, pref.width(), pref.height());
  layout.Layout(&host);
  ExpectViewBoundsEquals(0, 0, 45, 20, &v1);
  ExpectViewBoundsEquals(45, 0, 20, 20, &v2);
  ExpectViewBoundsEquals(65, 0, 20, 20, &v3);

  RemoveAll();
}

TEST_F(GridLayoutTest, ColSpan1) {
  View v1;
  v1.SetPreferredSize(gfx::Size(100, 20));
  View v2;
  v2.SetPreferredSize(gfx::Size(10, 40));
  ColumnSet* c1 = layout.AddColumnSet(0);
  c1->AddColumn(GridLayout::LEADING, GridLayout::LEADING,
                0, GridLayout::USE_PREF, 0, 0);
  c1->AddColumn(GridLayout::LEADING, GridLayout::LEADING,
                1, GridLayout::USE_PREF, 0, 0);
  layout.StartRow(0, 0);
  layout.AddView(&v1, 2, 1);
  layout.StartRow(0, 0);
  layout.AddView(&v2);

  GetPreferredSize();
  EXPECT_EQ(gfx::Size(100, 60), pref);

  host.SetBounds(0, 0, pref.width(), pref.height());
  layout.Layout(&host);
  ExpectViewBoundsEquals(0, 0, 100, 20, &v1);
  ExpectViewBoundsEquals(0, 20, 10, 40, &v2);

  RemoveAll();
}

TEST_F(GridLayoutTest, ColSpan2) {
  View v1;
  v1.SetPreferredSize(gfx::Size(100, 20));
  View v2;
  v2.SetPreferredSize(gfx::Size(10, 20));
  ColumnSet* c1 = layout.AddColumnSet(0);
  c1->AddColumn(GridLayout::LEADING, GridLayout::LEADING,
                1, GridLayout::USE_PREF, 0, 0);
  c1->AddColumn(GridLayout::LEADING, GridLayout::LEADING,
                0, GridLayout::USE_PREF, 0, 0);
  layout.StartRow(0, 0);
  layout.AddView(&v1, 2, 1);
  layout.StartRow(0, 0);
  layout.SkipColumns(1);
  layout.AddView(&v2);

  GetPreferredSize();
  EXPECT_EQ(gfx::Size(100, 40), pref);

  host.SetBounds(0, 0, pref.width(), pref.height());
  layout.Layout(&host);
  ExpectViewBoundsEquals(0, 0, 100, 20, &v1);
  ExpectViewBoundsEquals(90, 20, 10, 20, &v2);

  RemoveAll();
}

TEST_F(GridLayoutTest, ColSpan3) {
  View v1;
  v1.SetPreferredSize(gfx::Size(100, 20));
  View v2;
  v2.SetPreferredSize(gfx::Size(10, 20));
  View v3;
  v3.SetPreferredSize(gfx::Size(10, 20));
  ColumnSet* c1 = layout.AddColumnSet(0);
  c1->AddColumn(GridLayout::LEADING, GridLayout::LEADING,
                0, GridLayout::USE_PREF, 0, 0);
  c1->AddColumn(GridLayout::LEADING, GridLayout::LEADING,
                0, GridLayout::USE_PREF, 0, 0);
  layout.StartRow(0, 0);
  layout.AddView(&v1, 2, 1);
  layout.StartRow(0, 0);
  layout.AddView(&v2);
  layout.AddView(&v3);

  GetPreferredSize();
  EXPECT_EQ(gfx::Size(100, 40), pref);

  host.SetBounds(0, 0, pref.width(), pref.height());
  layout.Layout(&host);
  ExpectViewBoundsEquals(0, 0, 100, 20, &v1);
  ExpectViewBoundsEquals(0, 20, 10, 20, &v2);
  ExpectViewBoundsEquals(50, 20, 10, 20, &v3);

  RemoveAll();
}


TEST_F(GridLayoutTest, ColSpan4) {
  ColumnSet* set = layout.AddColumnSet(0);

  set->AddColumn(GridLayout::LEADING, GridLayout::LEADING, 0,
                 GridLayout::USE_PREF, 0, 0);
  set->AddColumn(GridLayout::LEADING, GridLayout::LEADING, 0,
                 GridLayout::USE_PREF, 0, 0);

  View v1;
  v1.SetPreferredSize(gfx::Size(10, 10));
  View v2;
  v2.SetPreferredSize(gfx::Size(10, 10));
  View v3;
  v3.SetPreferredSize(gfx::Size(25, 20));
  layout.StartRow(0, 0);
  layout.AddView(&v1);
  layout.AddView(&v2);
  layout.StartRow(0, 0);
  layout.AddView(&v3, 2, 1);

  GetPreferredSize();
  EXPECT_EQ(gfx::Size(25, 30), pref);

  host.SetBounds(0, 0, pref.width(), pref.height());
  layout.Layout(&host);
  ExpectViewBoundsEquals(0, 0, 10, 10, &v1);
  ExpectViewBoundsEquals(12, 0, 10, 10, &v2);
  ExpectViewBoundsEquals(0, 10, 25, 20, &v3);

  RemoveAll();
}

// Verifies the sizing of a view that doesn't start in the first column
// and has a column span > 1 (crbug.com/254092).
TEST_F(GridLayoutTest, ColSpanStartSecondColumn) {
  ColumnSet* set = layout.AddColumnSet(0);

  set->AddColumn(GridLayout::FILL, GridLayout::FILL, 0,
                 GridLayout::USE_PREF, 0, 0);
  set->AddColumn(GridLayout::FILL, GridLayout::FILL, 0,
                 GridLayout::USE_PREF, 0, 0);
  set->AddColumn(GridLayout::FILL, GridLayout::FILL, 0,
                 GridLayout::FIXED, 10, 0);

  View v1;
  v1.SetPreferredSize(gfx::Size(10, 10));
  View v2;
  v2.SetPreferredSize(gfx::Size(20, 10));

  layout.StartRow(0, 0);
  layout.AddView(&v1);
  layout.AddView(&v2, 2, 1);

  GetPreferredSize();
  EXPECT_EQ(gfx::Size(30, 10), pref);

  host.SetBounds(0, 0, pref.width(), pref.height());
  layout.Layout(&host);
  ExpectViewBoundsEquals(0, 0, 10, 10, &v1);
  ExpectViewBoundsEquals(10, 0, 20, 10, &v2);

  RemoveAll();
}

TEST_F(GridLayoutTest, SameSizeColumns) {
  View v1;
  v1.SetPreferredSize(gfx::Size(50, 20));
  View v2;
  v2.SetPreferredSize(gfx::Size(10, 10));
  ColumnSet* c1 = layout.AddColumnSet(0);
  c1->AddColumn(GridLayout::LEADING, GridLayout::LEADING,
                0, GridLayout::USE_PREF, 0, 0);
  c1->AddColumn(GridLayout::LEADING, GridLayout::LEADING,
                0, GridLayout::USE_PREF, 0, 0);
  c1->LinkColumnSizes(0, 1, -1);
  layout.StartRow(0, 0);
  layout.AddView(&v1);
  layout.AddView(&v2);

  gfx::Size pref = layout.GetPreferredSize(&host);
  EXPECT_EQ(gfx::Size(100, 20), pref);

  host.SetBounds(0, 0, pref.width(), pref.height());
  layout.Layout(&host);
  ExpectViewBoundsEquals(0, 0, 50, 20, &v1);
  ExpectViewBoundsEquals(50, 0, 10, 10, &v2);

  RemoveAll();
}

TEST_F(GridLayoutTest, HorizontalResizeTest1) {
  View v1;
  v1.SetPreferredSize(gfx::Size(50, 20));
  View v2;
  v2.SetPreferredSize(gfx::Size(10, 10));
  ColumnSet* c1 = layout.AddColumnSet(0);
  c1->AddColumn(GridLayout::FILL, GridLayout::LEADING,
                1, GridLayout::USE_PREF, 0, 0);
  c1->AddColumn(GridLayout::LEADING, GridLayout::LEADING,
                0, GridLayout::USE_PREF, 0, 0);
  layout.StartRow(0, 0);
  layout.AddView(&v1);
  layout.AddView(&v2);

  host.SetBounds(0, 0, 110, 20);
  layout.Layout(&host);
  ExpectViewBoundsEquals(0, 0, 100, 20, &v1);
  ExpectViewBoundsEquals(100, 0, 10, 10, &v2);

  RemoveAll();
}

TEST_F(GridLayoutTest, HorizontalResizeTest2) {
  View v1;
  v1.SetPreferredSize(gfx::Size(50, 20));
  View v2;
  v2.SetPreferredSize(gfx::Size(10, 10));
  ColumnSet* c1 = layout.AddColumnSet(0);
  c1->AddColumn(GridLayout::FILL, GridLayout::LEADING,
                1, GridLayout::USE_PREF, 0, 0);
  c1->AddColumn(GridLayout::TRAILING, GridLayout::LEADING,
                1, GridLayout::USE_PREF, 0, 0);
  layout.StartRow(0, 0);
  layout.AddView(&v1);
  layout.AddView(&v2);

  host.SetBounds(0, 0, 120, 20);
  layout.Layout(&host);
  ExpectViewBoundsEquals(0, 0, 80, 20, &v1);
  ExpectViewBoundsEquals(110, 0, 10, 10, &v2);

  RemoveAll();
}

// Tests that space leftover due to rounding is distributed to the last
// resizable column.
TEST_F(GridLayoutTest, HorizontalResizeTest3) {
  View v1;
  v1.SetPreferredSize(gfx::Size(10, 10));
  View v2;
  v2.SetPreferredSize(gfx::Size(10, 10));
  View v3;
  v3.SetPreferredSize(gfx::Size(10, 10));
  ColumnSet* c1 = layout.AddColumnSet(0);
  c1->AddColumn(GridLayout::FILL, GridLayout::LEADING,
                1, GridLayout::USE_PREF, 0, 0);
  c1->AddColumn(GridLayout::FILL, GridLayout::LEADING,
                1, GridLayout::USE_PREF, 0, 0);
  c1->AddColumn(GridLayout::TRAILING, GridLayout::LEADING,
                0, GridLayout::USE_PREF, 0, 0);
  layout.StartRow(0, 0);
  layout.AddView(&v1);
  layout.AddView(&v2);
  layout.AddView(&v3);

  host.SetBounds(0, 0, 31, 10);
  layout.Layout(&host);
  ExpectViewBoundsEquals(0, 0, 10, 10, &v1);
  ExpectViewBoundsEquals(10, 0, 11, 10, &v2);
  ExpectViewBoundsEquals(21, 0, 10, 10, &v3);

  RemoveAll();
}

TEST_F(GridLayoutTest, TestVerticalResize1) {
  View v1;
  v1.SetPreferredSize(gfx::Size(50, 20));
  View v2;
  v2.SetPreferredSize(gfx::Size(10, 10));
  ColumnSet* c1 = layout.AddColumnSet(0);
  c1->AddColumn(GridLayout::FILL, GridLayout::FILL,
                1, GridLayout::USE_PREF, 0, 0);
  layout.StartRow(1, 0);
  layout.AddView(&v1);
  layout.StartRow(0, 0);
  layout.AddView(&v2);

  GetPreferredSize();
  EXPECT_EQ(gfx::Size(50, 30), pref);

  host.SetBounds(0, 0, 50, 100);
  layout.Layout(&host);
  ExpectViewBoundsEquals(0, 0, 50, 90, &v1);
  ExpectViewBoundsEquals(0, 90, 50, 10, &v2);

  RemoveAll();
}

TEST_F(GridLayoutTest, Border) {
  host.SetBorder(CreateEmptyBorder(1, 2, 3, 4));
  View v1;
  v1.SetPreferredSize(gfx::Size(10, 20));
  ColumnSet* c1 = layout.AddColumnSet(0);
  c1->AddColumn(GridLayout::LEADING, GridLayout::LEADING,
                0, GridLayout::USE_PREF, 0, 0);
  layout.StartRow(0, 0);
  layout.AddView(&v1);

  GetPreferredSize();
  EXPECT_EQ(gfx::Size(16, 24), pref);

  host.SetBounds(0, 0, pref.width(), pref.height());
  layout.Layout(&host);
  ExpectViewBoundsEquals(2, 1, 10, 20, &v1);

  RemoveAll();
}

TEST_F(GridLayoutTest, FixedSize) {
  host.SetBorder(CreateEmptyBorder(2, 2, 2, 2));

  ColumnSet* set = layout.AddColumnSet(0);

  int column_count = 4;
  int title_width = 100;
  int row_count = 2;
  int pref_width = 10;
  int pref_height = 20;

  for (int i = 0; i < column_count; ++i) {
    set->AddColumn(GridLayout::CENTER,
                   GridLayout::CENTER,
                   0,
                   GridLayout::FIXED,
                   title_width,
                   title_width);
  }

  for (int row = 0; row < row_count; ++row) {
    layout.StartRow(0, 0);
    for (int col = 0; col < column_count; ++col) {
      layout.AddView(CreateSizedView(gfx::Size(pref_width, pref_height)));
    }
  }

  layout.Layout(&host);

  for (int i = 0; i < column_count; ++i) {
    for (int row = 0; row < row_count; ++row) {
      View* view = host.child_at(row * column_count + i);
      ExpectViewBoundsEquals(
          2 + title_width * i + (title_width - pref_width) / 2,
          2 + pref_height * row,
          pref_width,
          pref_height, view);
    }
  }

  GetPreferredSize();
  EXPECT_EQ(gfx::Size(column_count * title_width + 4,
                      row_count * pref_height + 4), pref);
}

TEST_F(GridLayoutTest, RowSpanWithPaddingRow) {
  ColumnSet* set = layout.AddColumnSet(0);

  set->AddColumn(GridLayout::CENTER,
                 GridLayout::CENTER,
                 0,
                 GridLayout::FIXED,
                 10,
                 10);

  layout.StartRow(0, 0);
  layout.AddView(CreateSizedView(gfx::Size(10, 10)), 1, 2);
  layout.AddPaddingRow(0, 10);
}

TEST_F(GridLayoutTest, RowSpan) {
  ColumnSet* set = layout.AddColumnSet(0);

  set->AddColumn(GridLayout::LEADING,
                 GridLayout::LEADING,
                 0,
                 GridLayout::USE_PREF,
                 0,
                 0);
  set->AddColumn(GridLayout::LEADING,
                 GridLayout::LEADING,
                 0,
                 GridLayout::USE_PREF,
                 0,
                 0);

  layout.StartRow(0, 0);
  layout.AddView(CreateSizedView(gfx::Size(20, 10)));
  layout.AddView(CreateSizedView(gfx::Size(20, 40)), 1, 2);
  layout.StartRow(1, 0);
  View* s3 = CreateSizedView(gfx::Size(20, 10));
  layout.AddView(s3);

  GetPreferredSize();
  EXPECT_EQ(gfx::Size(40, 40), pref);

  host.SetBounds(0, 0, pref.width(), pref.height());
  layout.Layout(&host);
  ExpectViewBoundsEquals(0, 10, 20, 10, s3);
}

TEST_F(GridLayoutTest, RowSpan2) {
  ColumnSet* set = layout.AddColumnSet(0);

  set->AddColumn(GridLayout::LEADING, GridLayout::LEADING,
                 0, GridLayout::USE_PREF, 0, 0);
  set->AddColumn(GridLayout::LEADING, GridLayout::LEADING,
                 0,GridLayout::USE_PREF, 0, 0);

  layout.StartRow(0, 0);
  layout.AddView(CreateSizedView(gfx::Size(20, 20)));
  View* s3 = CreateSizedView(gfx::Size(64, 64));
  layout.AddView(s3, 1, 3);

  layout.AddPaddingRow(0, 10);

  layout.StartRow(0, 0);
  layout.AddView(CreateSizedView(gfx::Size(10, 20)));

  GetPreferredSize();
  EXPECT_EQ(gfx::Size(84, 64), pref);

  host.SetBounds(0, 0, pref.width(), pref.height());
  layout.Layout(&host);
  ExpectViewBoundsEquals(20, 0, 64, 64, s3);
}

TEST_F(GridLayoutTest, FixedViewWidth) {
  ColumnSet* set = layout.AddColumnSet(0);

  set->AddColumn(GridLayout::LEADING, GridLayout::LEADING,
                 0, GridLayout::USE_PREF, 0, 0);
  set->AddColumn(GridLayout::LEADING, GridLayout::LEADING,
                 0,GridLayout::USE_PREF, 0, 0);

  layout.StartRow(0, 0);
  View* view = CreateSizedView(gfx::Size(30, 40));
  layout.AddView(view, 1, 1, GridLayout::LEADING, GridLayout::LEADING, 10, 0);

  GetPreferredSize();
  EXPECT_EQ(10, pref.width());
  EXPECT_EQ(40, pref.height());

  host.SetBounds(0, 0, pref.width(), pref.height());
  layout.Layout(&host);
  ExpectViewBoundsEquals(0, 0, 10, 40, view);
}

TEST_F(GridLayoutTest, FixedViewHeight) {
  ColumnSet* set = layout.AddColumnSet(0);

  set->AddColumn(GridLayout::LEADING, GridLayout::LEADING,
                 0, GridLayout::USE_PREF, 0, 0);
  set->AddColumn(GridLayout::LEADING, GridLayout::LEADING,
                 0,GridLayout::USE_PREF, 0, 0);

  layout.StartRow(0, 0);
  View* view = CreateSizedView(gfx::Size(30, 40));
  layout.AddView(view, 1, 1, GridLayout::LEADING, GridLayout::LEADING, 0, 10);

  GetPreferredSize();
  EXPECT_EQ(30, pref.width());
  EXPECT_EQ(10, pref.height());

  host.SetBounds(0, 0, pref.width(), pref.height());
  layout.Layout(&host);
  ExpectViewBoundsEquals(0, 0, 30, 10, view);
}

// Make sure that for views that span columns the underlying columns are resized
// based on the resize percent of the column.
TEST_F(GridLayoutTest, ColumnSpanResizing) {
  ColumnSet* set = layout.AddColumnSet(0);

  set->AddColumn(GridLayout::FILL, GridLayout::CENTER,
                 2, GridLayout::USE_PREF, 0, 0);
  set->AddColumn(GridLayout::FILL, GridLayout::CENTER,
                 4, GridLayout::USE_PREF, 0, 0);

  layout.StartRow(0, 0);
  // span_view spans two columns and is twice as big the views added below.
  View* span_view = CreateSizedView(gfx::Size(12, 40));
  layout.AddView(span_view, 2, 1, GridLayout::LEADING, GridLayout::LEADING);

  layout.StartRow(0, 0);
  View* view1 = CreateSizedView(gfx::Size(2, 40));
  View* view2 = CreateSizedView(gfx::Size(4, 40));
  layout.AddView(view1);
  layout.AddView(view2);

  host.SetBounds(0, 0, 12, 80);
  layout.Layout(&host);

  ExpectViewBoundsEquals(0, 0, 12, 40, span_view);

  // view1 should be 4 pixels wide
  // column_pref + (remaining_width * column_resize / total_column_resize) =
  // 2 + (6 * 2 / 6).
  ExpectViewBoundsEquals(0, 40, 4, 40, view1);

  // And view2 should be 8 pixels wide:
  // 4 + (6 * 4 / 6).
  ExpectViewBoundsEquals(4, 40, 8, 40, view2);
}

// Check that GetPreferredSize() takes resizing of columns into account when
// there is additional space in the case we have column sets of different
// preferred sizes.
TEST_F(GridLayoutTest, ColumnResizingOnGetPreferredSize) {
  ColumnSet* set = layout.AddColumnSet(0);
  set->AddColumn(GridLayout::FILL, GridLayout::CENTER,
                 1, GridLayout::USE_PREF, 0, 0);

  set = layout.AddColumnSet(1);
  set->AddColumn(GridLayout::FILL, GridLayout::CENTER,
                 1, GridLayout::USE_PREF, 0, 0);

  set = layout.AddColumnSet(2);
  set->AddColumn(GridLayout::FILL, GridLayout::CENTER,
                 1, GridLayout::USE_PREF, 0, 0);

  // Make a row containing a flexible view that trades width for height.
  layout.StartRow(0, 0);
  View* view1 = new FlexibleView(100);
  layout.AddView(view1, 1, 1, GridLayout::FILL, GridLayout::LEADING);

  // The second row contains a view of fixed size that will enforce a column
  // width of 20 pixels.
  layout.StartRow(0, 1);
  View* view2 = CreateSizedView(gfx::Size(20, 20));
  layout.AddView(view2, 1, 1, GridLayout::FILL, GridLayout::LEADING);

  // Add another flexible view in row three in order to ensure column set
  // ordering doesn't influence sizing behaviour.
  layout.StartRow(0, 2);
  View* view3 = new FlexibleView(40);
  layout.AddView(view3, 1, 1, GridLayout::FILL, GridLayout::LEADING);

  // We expect a height of 50: 30 from the variable width view in the first row
  // plus 20 from the statically sized view in the second row. The flexible
  // view in the third row should contribute no height.
  EXPECT_EQ(gfx::Size(20, 50), layout.GetPreferredSize(&host));
}

TEST_F(GridLayoutTest, MinimumPreferredSize) {
  View v1;
  v1.SetPreferredSize(gfx::Size(10, 20));
  ColumnSet* set = layout.AddColumnSet(0);
  set->AddColumn(GridLayout::FILL, GridLayout::FILL,
                 0, GridLayout::USE_PREF, 0, 0);
  layout.StartRow(0, 0);
  layout.AddView(&v1);

  GetPreferredSize();
  EXPECT_EQ(gfx::Size(10, 20), pref);

  layout.set_minimum_size(gfx::Size(40, 40));
  GetPreferredSize();
  EXPECT_EQ(gfx::Size(40, 40), pref);

  RemoveAll();
}

// Test that attempting a Layout() while nested in AddView() causes a DCHECK.
// GridLayout must guard against this as it hasn't yet updated the internal
// structures it uses to calculate Layout, so will give bogus results.
TEST_F(GridLayoutTest, LayoutOnAddDeath) {
  // gtest death tests, such as EXPECT_DCHECK_DEATH(), can not work in the
  // presence of fork() and other process launching. In views-mus, we have
  // already launched additional processes for our service manager. Performing
  // this test under mus is impossible.
  if (PlatformTestHelper::IsMus())
    return;

  // Don't use the |layout| data member from the test harness, otherwise
  // SetLayoutManager() can take not take ownership.
  GridLayout* grid_layout = new GridLayout(&host);
  host.SetLayoutManager(grid_layout);
  ColumnSet* set = grid_layout->AddColumnSet(0);
  set->AddColumn(GridLayout::FILL, GridLayout::FILL, 0, GridLayout::USE_PREF, 0,
                 0);
  grid_layout->StartRow(0, 0);
  LayoutOnAddView view;
  EXPECT_DCHECK_DEATH(grid_layout->AddView(&view));
  // Death tests use fork(), so nothing should be added here.
  EXPECT_FALSE(view.parent());

  // If the View has nothing to change, adding should succeed.
  view.set_target_size(view.GetPreferredSize());
  grid_layout->AddView(&view);
  EXPECT_TRUE(view.parent());

  RemoveAll();
}

}  // namespace views
