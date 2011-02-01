// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/examples/scroll_view_example.h"

#include "base/stringprintf.h"
#include "views/controls/button/radio_button.h"
#include "views/layout/grid_layout.h"
#include "views/view.h"

namespace examples {

// ScrollView's content, which draws gradient color on background.
// TODO(oshima): add child views as well.
class ScrollViewExample::ScrollableView : public views::View {
 public:
  ScrollableView() {
    SetColor(SK_ColorRED, SK_ColorCYAN);
    AddChildView(new views::TextButton(NULL, L"Button"));
    AddChildView(new views::RadioButton(L"Radio Button", 0));
  }

  virtual gfx::Size GetPreferredSize() {
    return gfx::Size(width(), height());
  }

  void SetColor(SkColor from, SkColor to) {
    set_background(
        views::Background::CreateVerticalGradientBackground(from, to));
  }

  void PlaceChildY(int index, int y) {
    views::View* view = GetChildViewAt(index);
    gfx::Size size = view->GetPreferredSize();
    view->SetBounds(0, y, size.width(), size.height());
  }

  virtual void Layout() {
    PlaceChildY(0, 0);
    PlaceChildY(1, height() / 2);
    SizeToPreferredSize();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScrollableView);
};

ScrollViewExample::ScrollViewExample(ExamplesMain* main)
    : ExampleBase(main) {
}

ScrollViewExample::~ScrollViewExample() {
}

std::wstring ScrollViewExample::GetExampleTitle() {
  return L"Scroll View";
}

void ScrollViewExample::CreateExampleView(views::View* container) {
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

void ScrollViewExample::ButtonPressed(views::Button* sender,
                                      const views::Event& event) {
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
    scroll_view_->ScrollContentsRegionToBeVisible(
        gfx::Rect(20, 500, 1000, 500));
  }
  scroll_view_->Layout();
}

}  // namespace examples
