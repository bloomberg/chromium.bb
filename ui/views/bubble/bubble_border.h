// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_BUBBLE_BUBBLE_BORDER_H_
#define UI_VIEWS_BUBBLE_BUBBLE_BORDER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/views/background.h"
#include "ui/views/border.h"

namespace gfx {
class ImageSkia;
}

namespace views {

// Renders a border, with optional arrow, and a custom dropshadow.
// This can be used to produce floating "bubble" objects with rounded corners.
class VIEWS_EXPORT BubbleBorder : public Border {
 public:
  // Possible locations for the (optional) arrow.
  // 0 bit specifies left or right.
  // 1 bit specifies top or bottom.
  // 2 bit specifies horizontal or vertical.
  // 3 bit specifies whether the arrow at the center of its residing edge.
  enum ArrowLocationMask {
    RIGHT    = 0x01,
    BOTTOM   = 0x02,
    VERTICAL = 0x04,
    CENTER   = 0x08,
  };

  enum ArrowLocation {
    TOP_LEFT      = 0,
    TOP_RIGHT     = RIGHT,
    BOTTOM_LEFT   = BOTTOM,
    BOTTOM_RIGHT  = BOTTOM | RIGHT,
    LEFT_TOP      = VERTICAL,
    RIGHT_TOP     = VERTICAL | RIGHT,
    LEFT_BOTTOM   = VERTICAL | BOTTOM,
    RIGHT_BOTTOM  = VERTICAL | BOTTOM | RIGHT,
    TOP_CENTER    = CENTER,
    BOTTOM_CENTER = CENTER | BOTTOM,
    LEFT_CENTER   = CENTER | VERTICAL,
    RIGHT_CENTER  = CENTER | VERTICAL | RIGHT,
    NONE  = 16,  // No arrow. Positioned under the supplied rect.
    FLOAT = 17,  // No arrow. Centered over the supplied rect.
  };

  enum Shadow {
    SHADOW    = 0,
    NO_SHADOW,
    BIG_SHADOW,
    SMALL_SHADOW,
    SHADOW_COUNT,
  };

  // The position of the bubble in relation to the anchor.
  enum BubbleAlignment {
    // The tip of the arrow points to the middle of the anchor.
    ALIGN_ARROW_TO_MID_ANCHOR,
    // The edge nearest to the arrow is lined up with the edge of the anchor.
    ALIGN_EDGE_TO_ANCHOR_EDGE
  };

  // The way the arrow should be painted.
  enum ArrowPaintType {
    // Fully render the arrow.
    PAINT_NORMAL,
    // Leave space for the arrow, but do not paint it.
    PAINT_TRANSPARENT,
    // Neither paint nor leave space for the arrow.
    PAINT_NONE,
  };

  BubbleBorder(ArrowLocation arrow, Shadow shadow, SkColor color);

  // Returns the radius of the corner of the border.
  // TODO(xiyuan): Get rid of this since it's part of BorderImages now?
  static int GetCornerRadius() {
    // We can't safely calculate a border radius by comparing the sizes of the
    // side and corner images, because either may have been extended in various
    // directions in order to do more subtle dropshadow fading or other effects.
    // So we hardcode the most accurate value.
    return 4;
  }

  // Sets the location for the arrow.
  void set_arrow_location(ArrowLocation loc) { arrow_location_ = loc; }
  ArrowLocation arrow_location() const { return arrow_location_; }

  // Sets the alignment.
  void set_alignment(BubbleAlignment alignment) { alignment_ = alignment; }
  BubbleAlignment alignment() const { return alignment_; }

  static ArrowLocation horizontal_mirror(ArrowLocation loc) {
    return (loc == TOP_CENTER || loc == BOTTOM_CENTER || loc >= NONE) ?
        loc : static_cast<ArrowLocation>(loc ^ RIGHT);
  }

  static ArrowLocation vertical_mirror(ArrowLocation loc) {
    return (loc == LEFT_CENTER || loc == RIGHT_CENTER || loc >= NONE) ?
        loc : static_cast<ArrowLocation>(loc ^ BOTTOM);
  }

  static bool has_arrow(ArrowLocation loc) {
    return loc < NONE;
  }

  static bool is_arrow_on_left(ArrowLocation loc) {
    return (loc == TOP_CENTER || loc == BOTTOM_CENTER || loc >= NONE) ?
        false : !(loc & RIGHT);
  }

  static bool is_arrow_on_top(ArrowLocation loc) {
    return (loc == LEFT_CENTER || loc == RIGHT_CENTER || loc >= NONE) ?
        false : !(loc & BOTTOM);
  }

  static bool is_arrow_on_horizontal(ArrowLocation loc) {
    return loc >= NONE ? false : !(loc & VERTICAL);
  }

  static bool is_arrow_at_center(ArrowLocation loc) {
    return has_arrow(loc) && !!(loc & CENTER);
  }

  // Sets the background color for the arrow body.  This is irrelevant if you do
  // not also set the arrow location to something other than NONE.
  void set_background_color(SkColor color) { background_color_ = color; }
  SkColor background_color() const { return background_color_; }

  void set_client_bounds(const gfx::Rect& bounds) { client_bounds_ = bounds; }
  const gfx::Rect& client_bounds() const { return client_bounds_; }

  // Sets a fixed offset for the arrow from the beginning of corresponding edge.
  // The arrow will still point to the same location but the bubble will shift
  // location to make that happen.
  void set_arrow_offset(int offset) { override_arrow_offset_ = offset; }

  // Sets the way the arrow is actually painted.  Default is PAINT_NORMAL.
  void set_paint_arrow(ArrowPaintType value) { arrow_paint_type_ = value; }

  // For borders with an arrow, gives the desired bounds (in screen coordinates)
  // given the rect to point to and the size of the contained contents.  This
  // depends on the arrow location, so if you change that, you should call this
  // again to find out the new coordinates.
  virtual gfx::Rect GetBounds(const gfx::Rect& position_relative_to,
                              const gfx::Size& contents_size) const;

  // Returns the corner radius of the current image set.
  int GetBorderCornerRadius() const;

  // Gets the arrow offset to use.
  int GetArrowOffset(const gfx::Size& border_size) const;

  // Overridden from Border:
  virtual gfx::Insets GetInsets() const OVERRIDE;

  // How many pixels the bubble border is from the edge of the images.
  virtual int GetBorderThickness() const;

 protected:
  virtual ~BubbleBorder();

  // Calculates the insets for a specific arrow location. Normally called from
  // GetInsets(arrow_location()), but may be called by specialized BubbleBorder
  // implementations.
  virtual gfx::Insets GetInsetsForArrowLocation(ArrowLocation arrow_loc) const;

 private:
  struct BorderImages;

  // Loads images if necessary.
  static BorderImages* GetBorderImages(Shadow shadow);

  int GetArrowSize() const;

  // Overridden from Border:
  virtual void Paint(const View& view, gfx::Canvas* canvas) OVERRIDE;

  void DrawEdgeWithArrow(gfx::Canvas* canvas,
                         bool is_horizontal,
                         const gfx::ImageSkia& edge,
                         const gfx::ImageSkia& arrow,
                         int start_x,
                         int start_y,
                         int before_arrow,
                         int after_arrow,
                         int offset) const;

  void DrawArrowInterior(gfx::Canvas* canvas, float tip_x, float tip_y) const;

  // Border graphics.
  struct BorderImages* images_;

  // Image bundles.
  static struct BorderImages* border_images_[SHADOW_COUNT];

  // Minimal offset of the arrow from the closet edge of bounding rect.
  int arrow_offset_;

  // If specified, overrides the pre-calculated |arrow_offset_| of the arrow.
  int override_arrow_offset_;

  ArrowLocation arrow_location_;
  ArrowPaintType arrow_paint_type_;
  BubbleAlignment alignment_;
  SkColor background_color_;

  // The client/content bounds; must be clipped from the background on Windows.
  // TODO(msw): Clean this up when Windows native controls are no longer needed.
  gfx::Rect client_bounds_;

  DISALLOW_COPY_AND_ASSIGN(BubbleBorder);
};

// A Background that clips itself to the specified BubbleBorder and uses
// the background color of the BubbleBorder.
class VIEWS_EXPORT BubbleBackground : public Background {
 public:
  explicit BubbleBackground(BubbleBorder* border) : border_(border) {}

  // Overridden from Background:
  virtual void Paint(gfx::Canvas* canvas, View* view) const OVERRIDE;

 private:
  BubbleBorder* border_;

  DISALLOW_COPY_AND_ASSIGN(BubbleBackground);
};

}  // namespace views

#endif  // UI_VIEWS_BUBBLE_BUBBLE_BORDER_H_
