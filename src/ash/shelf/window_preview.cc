// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/window_preview.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/wm/window_util.h"
#include "ui/aura/window.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"

namespace ash {

// The maximum height or width of a preview, and the maximum ratio.
constexpr int kPreviewMaxDimension = 192;
constexpr float kPreviewMaxRatio = 1.5;

// The margins around window titles.
constexpr int kTitleLineHeight = 20;
constexpr int kTitleMarginTop = 10;
constexpr int kTitleMarginBottom = 10;
constexpr int kTitleMarginRight = 16;

// The width and height of close buttons.
constexpr int kCloseButtonSize = 36;
constexpr int kCloseButtonImageSize = 24;
constexpr int kCloseButtonSideBleed = 8;
constexpr SkColor kCloseButtonColor = SK_ColorWHITE;

WindowPreview::WindowPreview(aura::Window* window,
                             Delegate* delegate,
                             const ui::NativeTheme* theme)
    : delegate_(delegate) {
  mirror_ =
      new wm::WindowMirrorView(window, /* trilinear_filtering_on_init=*/false);
  title_ = new views::Label(window->GetTitle());
  close_button_ = new views::ImageButton(this);

  AddChildView(mirror_);
  AddChildView(title_);
  AddChildView(close_button_);

  SetStyling(theme);
}

WindowPreview::~WindowPreview() = default;

void WindowPreview::SetStyling(const ui::NativeTheme* theme) {
  SkColor background_color =
      theme->GetSystemColor(ui::NativeTheme::kColorId_TooltipBackground);
  title_->SetEnabledColor(
      theme->GetSystemColor(ui::NativeTheme::kColorId_TooltipText));
  title_->SetBackgroundColor(background_color);

  // The background is not opaque, so we can't do subpixel rendering.
  title_->SetSubpixelRenderingEnabled(false);

  close_button_->SetImage(
      views::Button::STATE_NORMAL,
      gfx::CreateVectorIcon(kOverviewWindowCloseIcon, kCloseButtonColor));
  close_button_->SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                                   views::ImageButton::ALIGN_MIDDLE);
  close_button_->SetMinimumImageSize(
      gfx::Size(kCloseButtonImageSize, kCloseButtonImageSize));
}

gfx::Size WindowPreview::CalculatePreferredSize() const {
  gfx::Size mirror_size = mirror_->CalculatePreferredSize();
  float ratio = static_cast<float>(mirror_size.width()) /
                static_cast<float>(mirror_size.height());
  int preview_width;
  int preview_height;
  if (ratio > kPreviewMaxRatio) {
    // Very wide window.
    preview_width = kPreviewMaxDimension;
    preview_height = kPreviewMaxDimension / ratio;
  } else {
    preview_height = kPreviewMaxDimension;
    preview_width = preview_height * ratio;
  }

  int title_height_with_padding =
      kTitleLineHeight + kTitleMarginTop + kTitleMarginBottom;
  return gfx::Size(preview_width, preview_height + title_height_with_padding);
}

void WindowPreview::Layout() {
  gfx::Rect content_rect = GetContentsBounds();
  int tooltip_width = content_rect.width();
  int tooltip_height = content_rect.height();

  gfx::Size title_size = title_->CalculatePreferredSize();
  int title_height_with_padding =
      kTitleLineHeight + kTitleMarginTop + kTitleMarginBottom;
  int title_width = std::min(
      title_size.width(), tooltip_width - kCloseButtonSize - kTitleMarginRight);

  title_->SetBoundsRect(gfx::Rect(content_rect.x(),
                                  content_rect.y() + kTitleMarginTop,
                                  title_width, kTitleLineHeight));
  close_button_->SetBoundsRect(
      gfx::Rect(content_rect.right() - kCloseButtonSize + kCloseButtonSideBleed,
                content_rect.y(), kCloseButtonSize, kCloseButtonSize));
  mirror_->SetBoundsRect(
      gfx::Rect(content_rect.x(), content_rect.y() + title_height_with_padding,
                tooltip_width, tooltip_height - title_height_with_padding));
}

bool WindowPreview::OnMousePressed(const ui::MouseEvent& event) {
  if (!mirror_->bounds().Contains(event.location()))
    return false;

  aura::Window* source = mirror_->source();
  if (source) {
    // The window might have been closed in the mean time.
    // TODO: Use WindowObserver to listen to when previewed windows are
    // being closed and remove this condition.
    wm::ActivateWindow(source);

    // This will have the effect of deleting this view.
    delegate_->OnPreviewActivated(this);
  }
  return true;
}

void WindowPreview::ButtonPressed(views::Button* sender,
                                  const ui::Event& event) {
  // The close button was pressed.
  aura::Window* source = mirror_->source();

  // The window might have been closed in the mean time.
  // TODO: Use WindowObserver to listen to when previewed windows are
  // being closed and remove this condition.
  if (!source)
    return;
  wm::CloseWidgetForWindow(source);

  // This will have the effect of deleting this view.
  delegate_->OnPreviewDismissed(this);
}

}  // namespace ash
