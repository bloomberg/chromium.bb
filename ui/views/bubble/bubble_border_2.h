// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_BUBBLE_BUBBLE_BORDER_2_H_
#define UI_VIEWS_BUBBLE_BUBBLE_BORDER_2_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/gfx/shadow_value.h"
#include "ui/views/bubble/bubble_border.h"

namespace views {

// A BubbleBorder rendered with Skia drawing commands instead of images.
class VIEWS_EXPORT BubbleBorder2 : public BubbleBorder {
 public:
  explicit BubbleBorder2(ArrowLocation arrow_location);
  virtual ~BubbleBorder2();

  // Given the |bubble_rect| that this border encloses, and the bounds of the
  // anchor view |anchor_view_rect|, compute the right offset to place the
  // arrow at shifting the |bubble_rect| to fit inside the display area if
  // needed. Returns the shifted |bubble_rect|.
  gfx::Rect ComputeOffsetAndUpdateBubbleRect(gfx::Rect bubble_rect,
                                             const gfx::Rect& anchor_view_rect);

  // Returns the path in |mask| that would be created if this border were to be
  // applied to the rect specified by |bounds|.
  void GetMask(const gfx::Rect& bounds, gfx::Path* mask) const;

  void set_offset(const gfx::Point& offset) { offset_ = offset; }
  const gfx::Point& offset() const { return offset_; }

  void set_corner_radius(int corner_radius) { corner_radius_ = corner_radius; }
  int corner_radius() const { return corner_radius_; }

  void set_border_size(int border_size) { border_size_ = border_size; }
  int border_size() const { return border_size_; }

  void set_arrow_height(int arrow_height) { arrow_height_ = arrow_height; }
  int arrow_height() const { return arrow_height_; }

  void set_arrow_width(int arrow_width) { arrow_width_ = arrow_width; }
  int arrow_width() const { return arrow_width_; }

  void SetShadow(gfx::ShadowValue shadow);

  // views::BubbleBorder overrides:
  virtual int GetBorderThickness() const OVERRIDE;

 protected:
  virtual void PaintBackground(gfx::Canvas* canvas,
                               const gfx::Rect& bounds) const;

 private:
  // Gets arrow offset based on arrow location and |offset_|.
  int GetArrowOffset() const;

  // views::BubbleBorder overrides:
  virtual void GetInsets(gfx::Insets* insets) const OVERRIDE;
  virtual gfx::Rect GetBounds(const gfx::Rect& position_relative_to,
                              const gfx::Size& contents_size) const OVERRIDE;
  virtual void GetInsetsForArrowLocation(
      gfx::Insets* insets,
      ArrowLocation arrow_loc) const OVERRIDE;

  // views::Border overrides:
  virtual void Paint(const View& view,
                     gfx::Canvas* canvas) const OVERRIDE;

  int corner_radius_;
  int border_size_;
  int arrow_height_;
  int arrow_width_;
  SkColor background_color_;
  SkColor border_color_;

  // Offset in pixels by which the arrow is shifted relative the default middle
  // position. If the arrow is placed horizontally (at top or bottom), the |x_|
  // component of |offset_| specifies the offset, else, the |y_| component.
  gfx::Point offset_;

  gfx::ShadowValues shadows_;

  DISALLOW_COPY_AND_ASSIGN(BubbleBorder2);
};

}  // namespace views

#endif  // UI_VIEWS_BUBBLE_BUBBLE_BORDER_2_H_
