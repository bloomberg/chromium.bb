// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/examples/single_split_view_example.h"

#include "views/layout/grid_layout.h"

namespace examples {

SingleSplitViewExample::SingleSplitViewExample(ExamplesMain* main)
    : ExampleBase(main) {
}

SingleSplitViewExample::~SingleSplitViewExample() {
}

std::wstring SingleSplitViewExample::GetExampleTitle() {
  return L"Single Split View";
}

void SingleSplitViewExample::CreateExampleView(views::View* container) {
  SplittedView* splitted_view_1 = new SplittedView();
  SplittedView* splitted_view_2 = new SplittedView();

  single_split_view_ = new views::SingleSplitView(
      splitted_view_1, splitted_view_2,
      views::SingleSplitView::HORIZONTAL_SPLIT,
      NULL);

  splitted_view_1->SetColor(SK_ColorYELLOW, SK_ColorCYAN);

  views::GridLayout* layout = new views::GridLayout(container);
  container->SetLayoutManager(layout);

  // Add scroll view.
  views::ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, 0);
  layout->StartRow(1, 0);
  layout->AddView(single_split_view_);
}

SingleSplitViewExample::SplittedView::SplittedView() {
  SetColor(SK_ColorRED, SK_ColorGREEN);
}

SingleSplitViewExample::SplittedView::~SplittedView() {
}

void SingleSplitViewExample::SplittedView::SetColor(SkColor from, SkColor to) {
  set_background(
      views::Background::CreateVerticalGradientBackground(from, to));
}

gfx::Size SingleSplitViewExample::SplittedView::GetPreferredSize() {
  return gfx::Size(width(), height());
}

gfx::Size SingleSplitViewExample::SplittedView::GetMinimumSize() {
  return gfx::Size(10, 10);
}

void SingleSplitViewExample::SplittedView::Layout() {
  SizeToPreferredSize();
}

}  // namespace examples
