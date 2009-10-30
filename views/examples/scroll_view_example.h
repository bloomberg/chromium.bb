// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_EXAMPLES_SCROLL_BAR_EXAMPLE_H_
#define VIEWS_EXAMPLES_SCROLL_BAR_EXAMPLE_H_

#include "base/compiler_specific.h"
#include "base/string_util.h"
#include "views/controls/button/text_button.h"
#include "views/controls/scroll_view.h"
#include "views/examples/example_base.h"

namespace examples {

class ScrollViewExample : public ExampleBase,
                          public views::ButtonListener {
 public:
  explicit ScrollViewExample(ExamplesMain* main): ExampleBase(main) {}

  virtual ~ScrollViewExample() {}

  virtual std::wstring GetExampleTitle() {
    return L"Scroll View";
  }

  virtual void CreateExampleView(views::View* container) {
    wide_ = new views::TextButton(this, L"Wide");
    tall_ = new views::TextButton(this, L"Tall");
    big_square_ = new views::TextButton(this, L"Big Square");
    small_square_ = new views::TextButton(this, L"Small Square");
    scroll_to_ = new views::TextButton(this, L"Scroll to");
    scrollable_ = new ScrollableView();
    scroll_view_ = new views::ScrollView();
    scroll_view_->SetContents(scrollable_);
    scrollable_->SetBounds(0, 0, 1000, 100);
    scrollable_->SetColor(SK_ColorYELLOW, SK_ColorCYAN);

    views::GridLayout* layout = new views::GridLayout(container);
    container->SetLayoutManager(layout);

    // Add scroll view.
    views::ColumnSet* column_set = layout->AddColumnSet(0);
    column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                          views::GridLayout::USE_PREF, 0, 0);
    layout->StartRow(1, 0);
    layout->AddView(scroll_view_);

    // Add control buttons.
    column_set = layout->AddColumnSet(1);
    for (int i = 0; i < 5; i++) {
      column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                            views::GridLayout::USE_PREF, 0, 0);
    }
    layout->StartRow(0, 1);
    layout->AddView(wide_);
    layout->AddView(tall_);
    layout->AddView(big_square_);
    layout->AddView(small_square_);
    layout->AddView(scroll_to_);
  }

 private:
  // ScrollView's content, which draws gradient color on background.
  // TODO(oshima): add child views as well.
  class ScrollableView : public views::View {
   public:
    ScrollableView() {
      SetColor(SK_ColorRED, SK_ColorCYAN);
    }

    gfx::Size GetPreferredSize() {
      return gfx::Size(width(), height());
    }

    void SetColor(SkColor from, SkColor to) {
      set_background(
          views::Background::CreateVerticalGradientBackground(from, to));
    }

    virtual void Layout() {
      SizeToPreferredSize();
    }

   private:
    DISALLOW_COPY_AND_ASSIGN(ScrollableView);
  };

  // ButtonListner implementation.
  virtual void ButtonPressed(views::Button* sender, const views::Event& event) {
    if (sender == wide_) {
      scrollable_->SetBounds(0, 0, 1000, 100);
      scrollable_->SetColor(SK_ColorYELLOW, SK_ColorCYAN);
    } else if (sender == tall_) {
      scrollable_->SetBounds(0, 0, 100, 1000);
      scrollable_->SetColor(SK_ColorRED, SK_ColorCYAN);
    } else if (sender == big_square_) {
      scrollable_->SetBounds(0, 0, 1000, 1000);
      scrollable_->SetColor(SK_ColorRED, SK_ColorGREEN);
    } else if (sender == small_square_) {
      scrollable_->SetBounds(0, 0, 100, 100);
      scrollable_->SetColor(SK_ColorYELLOW, SK_ColorGREEN);
    } else if (sender == scroll_to_) {
      scroll_view_->ScrollContentsRegionToBeVisible(20, 500, 1000, 500);
    }
    scroll_view_->Layout();
  }

  // Control buttons to change the size of scrollable and jump to
  // predefined position.
  views::TextButton* wide_, *tall_, *big_square_, *small_square_, *scroll_to_;

  // The content of the scroll view.
  ScrollableView* scrollable_;

  // The scroll view to test.
  views::ScrollView* scroll_view_;

  DISALLOW_COPY_AND_ASSIGN(ScrollViewExample);
};

}  // namespace examples

#endif  // VIEWS_EXAMPLES_SCROLL_BAR_EXAMPLE_H_

