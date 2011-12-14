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
TabbedLauncherButton::ImageSet* TabbedLauncherButton::bg_image_1_ = NULL;
TabbedLauncherButton::ImageSet* TabbedLauncherButton::bg_image_2_ = NULL;
TabbedLauncherButton::ImageSet* TabbedLauncherButton::bg_image_3_ = NULL;

TabbedLauncherButton::TabbedLauncherButton(views::ButtonListener* listener,
                                           LauncherButtonHost* host)
    : views::ImageButton(listener),
      host_(host) {
  if (!bg_image_1_) {
    bg_image_1_ = CreateImageSet(IDR_AURA_LAUNCHER_TABBED_BROWSER_1,
                                 IDR_AURA_LAUNCHER_TABBED_BROWSER_1_PUSHED,
                                 IDR_AURA_LAUNCHER_TABBED_BROWSER_1_HOT);
    bg_image_2_ = CreateImageSet(IDR_AURA_LAUNCHER_TABBED_BROWSER_2,
                                 IDR_AURA_LAUNCHER_TABBED_BROWSER_2_PUSHED,
                                 IDR_AURA_LAUNCHER_TABBED_BROWSER_2_HOT);
    bg_image_3_ = CreateImageSet(IDR_AURA_LAUNCHER_TABBED_BROWSER_3,
                                 IDR_AURA_LAUNCHER_TABBED_BROWSER_3_PUSHED,
                                 IDR_AURA_LAUNCHER_TABBED_BROWSER_3_HOT);
  }
  SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                    views::ImageButton::ALIGN_MIDDLE);
}

TabbedLauncherButton::~TabbedLauncherButton() {
}

void TabbedLauncherButton::SetImages(const LauncherTabbedImages& images) {
  images_ = images;
  ImageSet* set;
  if (images_.size() <= 1)
    set = bg_image_1_;
  else if (images_.size() == 2)
    set = bg_image_2_;
  else
    set = bg_image_3_;
  SetImage(BS_NORMAL, set->normal_image);
  SetImage(BS_HOT, set->hot_image);
  SetImage(BS_PUSHED, set->pushed_image);
}

void TabbedLauncherButton::OnPaint(gfx::Canvas* canvas) {
  ImageButton::OnPaint(canvas);

  if (images_.empty())
    return;

  // Only show the first icon.
  // TODO(sky): if we settle on just 1 icon, then we should simplify surrounding
  // code (don't use a vector of images).
  int x = (width() - images_[0].image.width()) / 2;
  int y = (height() - images_[0].image.height()) / 2 + 1;
  canvas->DrawBitmapInt(images_[0].image, x, y);
}

bool TabbedLauncherButton::OnMousePressed(const views::MouseEvent& event) {
  ImageButton::OnMousePressed(event);
  host_->MousePressedOnButton(this, event);
  return true;
}

void TabbedLauncherButton::OnMouseReleased(const views::MouseEvent& event) {
  host_->MouseReleasedOnButton(this, false);
  ImageButton::OnMouseReleased(event);
}

void TabbedLauncherButton::OnMouseCaptureLost() {
  host_->MouseReleasedOnButton(this, true);
  ImageButton::OnMouseCaptureLost();
}

bool TabbedLauncherButton::OnMouseDragged(const views::MouseEvent& event) {
  ImageButton::OnMouseDragged(event);
  host_->MouseDraggedOnButton(this, event);
  return true;
}

// static
TabbedLauncherButton::ImageSet* TabbedLauncherButton::CreateImageSet(
    int normal_id,
    int pushed_id,
    int hot_id) {
  ImageSet* set = new ImageSet;
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  set->normal_image = new SkBitmap(*rb.GetImageNamed(normal_id).ToSkBitmap());
  set->pushed_image = new SkBitmap(*rb.GetImageNamed(pushed_id).ToSkBitmap());
  set->hot_image = new SkBitmap(*rb.GetImageNamed(hot_id).ToSkBitmap());
  return set;
}

}  // namespace internal
}  // namespace aura_shell
