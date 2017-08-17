// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_TILE_ITEM_VIEW_H_
#define UI_APP_LIST_VIEWS_TILE_ITEM_VIEW_H_

#include <memory>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/app_list/app_list_export.h"
#include "ui/app_list/views/image_shadow_animator.h"
#include "ui/views/controls/button/button.h"

namespace gfx {
class ImageSkia;
}

namespace views {
class ImageView;
class Label;
}

namespace app_list {

// The view for a tile in the app list on the start/search page.
// TODO(warx): Merge this class to its subclass SearchResultTileItemView once
// bubble launcher deprecates.
class APP_LIST_EXPORT TileItemView : public views::Button,
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

  // Overridden from views::Button:
  void StateChanged(ButtonState old_state) override;
  void PaintButtonContents(gfx::Canvas* canvas) override;

  // Overridden from views::View:
  void Layout() override;
  const char* GetClassName() const override;

  // Overridden from ImageShadowAnimator::Delegate:
  void ImageShadowAnimationProgressed(ImageShadowAnimator* animator) override;

 protected:
  // Overridden from views::View:
  gfx::Size CalculatePreferredSize() const override;
  bool GetTooltipText(const gfx::Point& p,
                      base::string16* tooltip) const override;

  views::ImageView* icon() const { return icon_; }
  void SetIcon(const gfx::ImageSkia& icon);

  views::ImageView* badge() const { return badge_; }
  void SetBadgeIcon(const gfx::ImageSkia& badge_icon);

  views::Label* title() const { return title_; }
  void SetTitle(const base::string16& title);

  void set_is_recommendation(bool is_recommendation) {
    is_recommendation_ = is_recommendation;
  }

 private:
  void UpdateBackgroundColor();

  SkColor parent_background_color_;
  std::unique_ptr<ImageShadowAnimator> image_shadow_animator_;

  views::ImageView* icon_;   // Owned by views hierarchy.
  views::ImageView* badge_;  // Owned by views hierarchy.
  views::Label* title_;      // Owned by views hierarchy.

  bool selected_ = false;

  // Indicates whether this view is the base class for recommendation display
  // type SearchResultTileItemView.
  // TODO(warx): It is not needed once TileItemView class is merged to
  // SearchResultTileItemVIew.
  bool is_recommendation_ = false;

  const bool is_fullscreen_app_list_enabled_;

  DISALLOW_COPY_AND_ASSIGN(TileItemView);
};

}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_TILE_ITEM_VIEW_H_
