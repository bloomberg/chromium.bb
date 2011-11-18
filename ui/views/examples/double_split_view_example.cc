// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/double_split_view_example.h"

#include "ui/views/layout/grid_layout.h"
#include "views/controls/single_split_view.h"

namespace {

// DoubleSplitViews's content, which draws gradient color on background.
class SplittedView : public views::View {
 public:
  SplittedView();
  virtual ~SplittedView();

  void SetColor(SkColor from, SkColor to);

  // Overridden from views::View.
  virtual gfx::Size GetMinimumSize() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(SplittedView);
};

SplittedView::SplittedView() {
  SetColor(SK_ColorRED, SK_ColorGREEN);
}

SplittedView::~SplittedView() {
}

void SplittedView::SetColor(SkColor from, SkColor to) {
  set_background(
      views::Background::CreateVerticalGradientBackground(from, to));
}

gfx::Size SplittedView::GetMinimumSize() {
  return gfx::Size(10, 10);
}

}  // namespace

namespace examples {

DoubleSplitViewExample::DoubleSplitViewExample(ExamplesMain* main)
    : ExampleBase(main, "Double Split View") {
}

DoubleSplitViewExample::~DoubleSplitViewExample() {
}

void DoubleSplitViewExample::CreateExampleView(views::View* container) {
  SplittedView* splitted_view_1 = new SplittedView();
  SplittedView* splitted_view_2 = new SplittedView();
  SplittedView* splitted_view_3 = new SplittedView();

  inner_single_split_view_ = new views::SingleSplitView(
      splitted_view_1, splitted_view_2,
      views::SingleSplitView::HORIZONTAL_SPLIT,
      NULL);

  outer_single_split_view_ = new views::SingleSplitView(
      inner_single_split_view_, splitted_view_3,
      views::SingleSplitView::HORIZONTAL_SPLIT,
      NULL);

  views::GridLayout* layout = new views::GridLayout(container);
  container->SetLayoutManager(layout);

  // Add scroll view.
  views::ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, 0);
  layout->StartRow(1, 0);
  layout->AddView(outer_single_split_view_);
}

}  // namespace examples
