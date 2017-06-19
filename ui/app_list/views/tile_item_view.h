// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_TILE_ITEM_VIEW_H_
#define UI_APP_LIST_VIEWS_TILE_ITEM_VIEW_H_

#include "base/macros.h"
#include "base/strings/string16.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/app_list/app_list_export.h"
#include "ui/app_list/views/image_shadow_animator.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/custom_button.h"

namespace gfx {
class ImageSkia;
}

namespace views {
class ImageView;
class Label;
}

namespace app_list {

// The view for a tile in the app list on the start/search page.
class APP_LIST_EXPORT TileItemView : public views::CustomButton,
                                     public views::ButtonListener,
                                     public ImageShadowAnimator::Delegate {
 public:
  enum HoverStyle {
    HOVER_STYLE_ANIMATE_SHADOW,
    HOVER_STYLE_DARKEN_BACKGROUND,
  };

  TileItemView();
  ~TileItemView() override;

  bool selected() { return selected_; }
  void SetSelected(bool selected);

  // Informs the TileItemView of its parent's background color. The controls
  // within the TileItemView will adapt to suit the given color.
  void SetParentBackgroundColor(SkColor color);
  SkColor parent_background_color() { return parent_background_color_; }

  // Sets the behavior of the tile item on mouse hover.
  void SetHoverStyle(HoverStyle hover_style);

  // Overridden from views::CustomButton:
  void StateChanged(ButtonState old_state) override;

  // Overridden from views::View:
  void Layout() override;
  const char* GetClassName() const override;

  // Overridden from ImageShadowAnimator::Delegate:
  void ImageShadowAnimationProgressed(ImageShadowAnimator* animator) override;

 protected:
  void SetIcon(const gfx::ImageSkia& icon);
  void SetBadgeIcon(const gfx::ImageSkia& badge_icon);
  void SetTitle(const base::string16& title);

 private:
  void UpdateBackgroundColor();

  // Overridden from views::View:
  gfx::Size CalculatePreferredSize() const override;
  bool GetTooltipText(const gfx::Point& p,
                      base::string16* tooltip) const override;

  SkColor parent_background_color_;
  std::unique_ptr<ImageShadowAnimator> image_shadow_animator_;

  views::ImageView* icon_;   // Owned by views hierarchy.
  views::ImageView* badge_;  // Owned by views hierarchy.
  views::Label* title_;      // Owned by views hierarchy.

  bool selected_ = false;

  DISALLOW_COPY_AND_ASSIGN(TileItemView);
};

}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_TILE_ITEM_VIEW_H_
