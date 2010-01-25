// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_EXAMPLES_SINGLE_SPLIT_VIEW_EXAMPLE_H_
#define VIEWS_EXAMPLES_SINGLE_SPLIT_VIEW_EXAMPLE_H_

#include "views/controls/single_split_view.h"
#include "views/examples/example_base.h"

namespace examples {

class SingleSplitViewExample : public ExampleBase {
 public:
  explicit SingleSplitViewExample(ExamplesMain* main)
      : ExampleBase(main) {
  }

  virtual ~SingleSplitViewExample() {}

  virtual std::wstring GetExampleTitle() {
    return L"Single Split View";
  }

  virtual void CreateExampleView(views::View* container) {
    SplittedView* splitted_view_1 = new SplittedView();
    SplittedView* splitted_view_2 = new SplittedView();

    single_split_view_ = new views::SingleSplitView(
        splitted_view_1, splitted_view_2,
        views::SingleSplitView::HORIZONTAL_SPLIT);

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

 private:
  // SingleSplitView's content, which draws gradient color on background.
  class SplittedView : public views::View {
   public:
    SplittedView() {
      SetColor(SK_ColorRED, SK_ColorGREEN);
    }

    virtual gfx::Size GetPreferredSize() {
      return gfx::Size(width(), height());
    }

    virtual gfx::Size GetMinimumSize() {
      return gfx::Size(10, 10);
    }

    void SetColor(SkColor from, SkColor to) {
      set_background(
          views::Background::CreateVerticalGradientBackground(from, to));
    }

    virtual void Layout() {
      SizeToPreferredSize();
    }

   private:
    DISALLOW_COPY_AND_ASSIGN(SplittedView);
  };

  // The SinleSplitView to test.
  views::SingleSplitView* single_split_view_;

  DISALLOW_COPY_AND_ASSIGN(SingleSplitViewExample);
};

}  // namespace examples

#endif  // VIEWS_EXAMPLES_SINGLE_SPLIT_VIEW_EXAMPLE_H_
