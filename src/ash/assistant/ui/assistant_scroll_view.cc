// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/assistant_scroll_view.h"

#include "ui/views/controls/scrollbar/overlay_scroll_bar.h"

namespace ash {

namespace {

// ContentView ----------------------------------------------------------------

class ContentView : public views::View, views::ViewObserver {
 public:
  ContentView() { AddObserver(this); }

  ~ContentView() override { RemoveObserver(this); }

  // views::View:
  void ChildPreferredSizeChanged(views::View* child) override {
    PreferredSizeChanged();
  }

  void ChildVisibilityChanged(views::View* child) override {
    PreferredSizeChanged();
  }

  // views::ViewObserver:
  void OnChildViewAdded(views::View* view, views::View* child) override {
    PreferredSizeChanged();
  }

  void OnChildViewRemoved(views::View* view, views::View* child) override {
    PreferredSizeChanged();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentView);
};

// InvisibleScrollBar ----------------------------------------------------------

class InvisibleScrollBar : public views::OverlayScrollBar {
 public:
  InvisibleScrollBar(bool horizontal) : views::OverlayScrollBar(horizontal) {}

  ~InvisibleScrollBar() override = default;

  // views::OverlayScrollBar:
  int GetThickness() const override { return 0; }

 private:
  DISALLOW_COPY_AND_ASSIGN(InvisibleScrollBar);
};

}  // namespace

// AssistantScrollView ---------------------------------------------------------

AssistantScrollView::AssistantScrollView() {
  InitLayout();
}

AssistantScrollView::~AssistantScrollView() = default;

void AssistantScrollView::OnViewPreferredSizeChanged(views::View* view) {
  OnContentsPreferredSizeChanged(content_view_);
  PreferredSizeChanged();
}

void AssistantScrollView::InitLayout() {
  SetBackgroundColor(SK_ColorTRANSPARENT);
  set_draw_overflow_indicator(false);

  // Content view.
  content_view_ = new ContentView();
  content_view_->AddObserver(this);
  SetContents(content_view_);

  // Scroll bars.
  horizontal_scroll_bar_ = new InvisibleScrollBar(/*horizontal=*/true);
  SetHorizontalScrollBar(horizontal_scroll_bar_);

  vertical_scroll_bar_ = new InvisibleScrollBar(/*horizontal=*/false);
  SetVerticalScrollBar(vertical_scroll_bar_);
}

}  // namespace ash
