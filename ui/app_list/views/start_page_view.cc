// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/start_page_view.h"

#include "content/public/browser/web_contents.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/views/app_list_main_view.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/box_layout.h"

namespace app_list {

namespace {

const int kTopMargin = 20;

const int kWebViewWidth = 200;
const int kWebViewHeight = 95;

const int kInstantContainerSpacing = 15;
const int kBarPlaceholderWidth = 350;
const int kBarPlaceholderHeight = 30;

// A button that is the placeholder for the search bar in the start page view.
class BarPlaceholderButton : public views::CustomButton {
 public:
  explicit BarPlaceholderButton(views::ButtonListener* listener)
      : views::CustomButton(listener) {}

  virtual ~BarPlaceholderButton() {}

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize() const OVERRIDE {
    return gfx::Size(kBarPlaceholderWidth, kBarPlaceholderHeight);
  }

  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    PaintButton(
        canvas,
        state() == STATE_HOVERED ? kPagerHoverColor : kPagerNormalColor);
  }

 private:
  // Paints a rectangular button.
  void PaintButton(gfx::Canvas* canvas, SkColor base_color) {
    gfx::Rect rect(GetContentsBounds());
    rect.ClampToCenteredSize(
        gfx::Size(kBarPlaceholderWidth, kBarPlaceholderHeight));

    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setStyle(SkPaint::kFill_Style);
    paint.setColor(base_color);
    canvas->DrawRect(rect, paint);
  }

  DISALLOW_COPY_AND_ASSIGN(BarPlaceholderButton);
};

}  // namespace

StartPageView::StartPageView(AppListMainView* app_list_main_view,
                             content::WebContents* start_page_web_contents)
    : app_list_main_view_(app_list_main_view),
      instant_container_(new views::View) {
  AddChildView(instant_container_);
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0));
  instant_container_->SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kVertical, 0, kTopMargin, kInstantContainerSpacing));

  views::WebView* web_view =
      new views::WebView(start_page_web_contents->GetBrowserContext());
  web_view->SetPreferredSize(gfx::Size(kWebViewWidth, kWebViewHeight));
  web_view->SetWebContents(start_page_web_contents);

  instant_container_->AddChildView(web_view);
  instant_container_->AddChildView(new BarPlaceholderButton(this));
}

StartPageView::~StartPageView() {
}

void StartPageView::Reset() {
  instant_container_->SetVisible(true);
}

void StartPageView::ButtonPressed(views::Button* sender,
                                  const ui::Event& event) {
  app_list_main_view_->OnStartPageSearchButtonPressed();
  instant_container_->SetVisible(false);
}

}  // namespace app_list
