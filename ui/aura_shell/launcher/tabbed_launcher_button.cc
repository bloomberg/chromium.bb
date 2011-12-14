// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/launcher/tabbed_launcher_button.h"

#include <algorithm>

#include "grit/ui_resources.h"
#include "ui/aura_shell/launcher/launcher_button_host.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/insets.h"

namespace aura_shell {
namespace internal {

// The images drawn inside the background tab are drawn at this offset from
// the edge.
const int kBgImageContentInset = 12;

// Padding between each of the images.
const int kImagePadding = 8;

// Insets used in painting the background if it's rendered bigger than the size
// of the background image. See ImagePainter::CreateImagePainter for how these
// are interpreted.
const int kBgTopInset = 12;
const int kBgLeftInset = 30;
const int kBgBottomInset = 12;
const int kBgRightInset = 8;

// static
SkBitmap* TabbedLauncherButton::bg_image_1_ = NULL;
SkBitmap* TabbedLauncherButton::bg_image_2_ = NULL;
SkBitmap* TabbedLauncherButton::bg_image_3_ = NULL;

TabbedLauncherButton::TabbedLauncherButton(views::ButtonListener* listener,
                                           LauncherButtonHost* host)
    : views::CustomButton(listener),
      host_(host) {
  if (!bg_image_1_) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    bg_image_1_ = new SkBitmap(
        *rb.GetImageNamed(IDR_AURA_LAUNCHER_TABBED_BROWSER_1).ToSkBitmap());
    bg_image_2_ = new SkBitmap(
        *rb.GetImageNamed(IDR_AURA_LAUNCHER_TABBED_BROWSER_2).ToSkBitmap());
    bg_image_3_ = new SkBitmap(
        *rb.GetImageNamed(IDR_AURA_LAUNCHER_TABBED_BROWSER_3).ToSkBitmap());
  }
}

TabbedLauncherButton::~TabbedLauncherButton() {
}

void TabbedLauncherButton::SetImages(const LauncherTabbedImages& images) {
  images_ = images;
}

gfx::Size TabbedLauncherButton::GetPreferredSize() {
  return gfx::Size(bg_image_1_->width(), bg_image_1_->height());
}

void TabbedLauncherButton::OnPaint(gfx::Canvas* canvas) {
  SkBitmap* bg_image = NULL;
  if (images_.size() <= 1)
    bg_image = bg_image_1_;
  else if (images_.size() == 2)
    bg_image = bg_image_2_;
  else
    bg_image = bg_image_3_;
  canvas->DrawBitmapInt(*bg_image, 0, 0);

  if (images_.empty())
    return;

  // Only show the first icon.
  // TODO(sky): if we settle on just 1 icon, then we should simplify surrounding
  // code (don't use a vector of images).
  int x = (width() - images_[0].image.width()) / 2;
  int y = (height() - images_[0].image.height()) / 2;
  canvas->DrawBitmapInt(images_[0].image, x, y);
}

bool TabbedLauncherButton::OnMousePressed(const views::MouseEvent& event) {
  CustomButton::OnMousePressed(event);
  host_->MousePressedOnButton(this, event);
  return true;
}

void TabbedLauncherButton::OnMouseReleased(const views::MouseEvent& event) {
  host_->MouseReleasedOnButton(this, false);
  CustomButton::OnMouseReleased(event);
}

void TabbedLauncherButton::OnMouseCaptureLost() {
  host_->MouseReleasedOnButton(this, true);
  CustomButton::OnMouseCaptureLost();
}

bool TabbedLauncherButton::OnMouseDragged(const views::MouseEvent& event) {
  CustomButton::OnMouseDragged(event);
  host_->MouseDraggedOnButton(this, event);
  return true;
}

}  // namespace internal
}  // namespace aura_shell
