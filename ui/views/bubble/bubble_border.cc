// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/bubble/bubble_border.h"

#include <algorithm>  // for std::max

#include "base/logging.h"
#include "grit/ui_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/skia_util.h"

namespace {

// Stroke size in pixels of borders in image assets.
const int kBorderStrokeSize = 1;

// Border image resource ids.
const int kShadowImages[] = {
    IDR_BUBBLE_SHADOW_L,
    IDR_BUBBLE_SHADOW_TL,
    IDR_BUBBLE_SHADOW_T,
    IDR_BUBBLE_SHADOW_TR,
    IDR_BUBBLE_SHADOW_R,
    IDR_BUBBLE_SHADOW_BR,
    IDR_BUBBLE_SHADOW_B,
    IDR_BUBBLE_SHADOW_BL,
};

const int kNoShadowImages[] = {
    IDR_BUBBLE_L,
    IDR_BUBBLE_TL,
    IDR_BUBBLE_T,
    IDR_BUBBLE_TR,
    IDR_BUBBLE_R,
    IDR_BUBBLE_BR,
    IDR_BUBBLE_B,
    IDR_BUBBLE_BL,
    IDR_BUBBLE_L_ARROW,
    IDR_BUBBLE_T_ARROW,
    IDR_BUBBLE_R_ARROW,
    IDR_BUBBLE_B_ARROW,
};

const int kBigShadowImages[] = {
    IDR_WINDOW_BUBBLE_SHADOW_BIG_LEFT,
    IDR_WINDOW_BUBBLE_SHADOW_BIG_TOP_LEFT,
    IDR_WINDOW_BUBBLE_SHADOW_BIG_TOP,
    IDR_WINDOW_BUBBLE_SHADOW_BIG_TOP_RIGHT,
    IDR_WINDOW_BUBBLE_SHADOW_BIG_RIGHT,
    IDR_WINDOW_BUBBLE_SHADOW_BIG_BOTTOM_RIGHT,
    IDR_WINDOW_BUBBLE_SHADOW_BIG_BOTTOM,
    IDR_WINDOW_BUBBLE_SHADOW_BIG_BOTTOM_LEFT,
    IDR_WINDOW_BUBBLE_SHADOW_SPIKE_BIG_LEFT,
    IDR_WINDOW_BUBBLE_SHADOW_SPIKE_BIG_TOP,
    IDR_WINDOW_BUBBLE_SHADOW_SPIKE_BIG_RIGHT,
    IDR_WINDOW_BUBBLE_SHADOW_SPIKE_BIG_BOTTOM,
};

const int kSmallShadowImages[] = {
    IDR_WINDOW_BUBBLE_SHADOW_SMALL_LEFT,
    IDR_WINDOW_BUBBLE_SHADOW_SMALL_TOP_LEFT,
    IDR_WINDOW_BUBBLE_SHADOW_SMALL_TOP,
    IDR_WINDOW_BUBBLE_SHADOW_SMALL_TOP_RIGHT,
    IDR_WINDOW_BUBBLE_SHADOW_SMALL_RIGHT,
    IDR_WINDOW_BUBBLE_SHADOW_SMALL_BOTTOM_RIGHT,
    IDR_WINDOW_BUBBLE_SHADOW_SMALL_BOTTOM,
    IDR_WINDOW_BUBBLE_SHADOW_SMALL_BOTTOM_LEFT,
    IDR_WINDOW_BUBBLE_SHADOW_SPIKE_SMALL_LEFT,
    IDR_WINDOW_BUBBLE_SHADOW_SPIKE_SMALL_TOP,
    IDR_WINDOW_BUBBLE_SHADOW_SPIKE_SMALL_RIGHT,
    IDR_WINDOW_BUBBLE_SHADOW_SPIKE_SMALL_BOTTOM,
};

}  // namespace

namespace views {

struct BubbleBorder::BorderImages {
  BorderImages(const int image_ids[],
               size_t image_ids_size,
               int arrow_interior_height,
               int border_thickness,
               int corner_radius)
      : arrow_interior_height(arrow_interior_height),
        border_thickness(border_thickness),
        corner_radius(corner_radius) {
    // Only two possible sizes of image ids array.
    DCHECK(image_ids_size == 12 || image_ids_size == 8);

    ResourceBundle& rb = ResourceBundle::GetSharedInstance();

    left = *rb.GetImageSkiaNamed(image_ids[0]);
    top_left = *rb.GetImageSkiaNamed(image_ids[1]);
    top = *rb.GetImageSkiaNamed(image_ids[2]);
    top_right = *rb.GetImageSkiaNamed(image_ids[3]);
    right = *rb.GetImageSkiaNamed(image_ids[4]);
    bottom_right = *rb.GetImageSkiaNamed(image_ids[5]);
    bottom = *rb.GetImageSkiaNamed(image_ids[6]);
    bottom_left = *rb.GetImageSkiaNamed(image_ids[7]);

    if (image_ids_size > 8) {
      left_arrow = *rb.GetImageSkiaNamed(image_ids[8]);
      top_arrow = *rb.GetImageSkiaNamed(image_ids[9]);
      right_arrow = *rb.GetImageSkiaNamed(image_ids[10]);
      bottom_arrow = *rb.GetImageSkiaNamed(image_ids[11]);
    }
  }

  gfx::ImageSkia left;
  gfx::ImageSkia top_left;
  gfx::ImageSkia top;
  gfx::ImageSkia top_right;
  gfx::ImageSkia right;
  gfx::ImageSkia bottom_right;
  gfx::ImageSkia bottom;
  gfx::ImageSkia bottom_left;

  gfx::ImageSkia left_arrow;
  gfx::ImageSkia top_arrow;
  gfx::ImageSkia right_arrow;
  gfx::ImageSkia bottom_arrow;

  int arrow_interior_height;  // The height inside the arrow image, in pixels.
  int border_thickness;
  int corner_radius;
};

// static
struct BubbleBorder::BorderImages*
    BubbleBorder::border_images_[SHADOW_COUNT] = { NULL };

BubbleBorder::BubbleBorder(ArrowLocation arrow_location, Shadow shadow)
    : override_arrow_offset_(0),
      arrow_location_(arrow_location),
      paint_arrow_(true),
      alignment_(ALIGN_ARROW_TO_MID_ANCHOR),
      background_color_(SK_ColorWHITE) {
  DCHECK(shadow < SHADOW_COUNT);
  images_ = GetBorderImages(shadow);

  // Calculate horizontal and vertical insets for arrow by ensuring that
  // the widest arrow and corner images will have enough room to avoid overlap
  int offset_x =
      (std::max(images_->top_arrow.width(),
                images_->bottom_arrow.width()) / 2) +
      std::max(std::max(images_->top_left.width(),
                        images_->top_right.width()),
               std::max(images_->bottom_left.width(),
                        images_->bottom_right.width()));
  int offset_y =
      (std::max(images_->left_arrow.height(),
                images_->right_arrow.height()) / 2) +
      std::max(std::max(images_->top_left.height(),
                        images_->top_right.height()),
               std::max(images_->bottom_left.height(),
                        images_->bottom_right.height()));
  arrow_offset_ = std::max(offset_x, offset_y);
}

gfx::Rect BubbleBorder::GetBounds(const gfx::Rect& position_relative_to,
                                  const gfx::Size& contents_size) const {
  // Desired size is size of contents enlarged by the size of the border images.
  gfx::Size border_size(contents_size);
  gfx::Insets insets = GetInsets();
  border_size.Enlarge(insets.width(), insets.height());

  // Ensure the bubble has a minimum size that draws arrows correctly.
  if (is_arrow_on_horizontal(arrow_location_))
    border_size.set_width(std::max(border_size.width(), 2 * arrow_offset_));
  else if (has_arrow(arrow_location_))
    border_size.set_height(std::max(border_size.height(), 2 * arrow_offset_));

  // Screen position depends on the arrow location.
  // The bubble should overlap the target by some amount since there is space
  // for shadow between arrow tip/bubble border and image bounds.
  int x = position_relative_to.x();
  int y = position_relative_to.y();
  int w = position_relative_to.width();
  int h = position_relative_to.height();

  const int arrow_size = images_->arrow_interior_height + kBorderStrokeSize;
  const int arrow_offset = GetArrowOffset(border_size);

  // Calculate bubble x coordinate.
  switch (arrow_location_) {
    case TOP_LEFT:
    case BOTTOM_LEFT:
      x += alignment_ == ALIGN_ARROW_TO_MID_ANCHOR ? w / 2 - arrow_offset :
           -(images_->left.width() - kBorderStrokeSize);
      break;

    case TOP_RIGHT:
    case BOTTOM_RIGHT:
      x += alignment_ == ALIGN_ARROW_TO_MID_ANCHOR ?
          w / 2 + arrow_offset - border_size.width() + 1 :
          w - border_size.width() + images_->right.width() - kBorderStrokeSize;
      break;

    case LEFT_TOP:
    case LEFT_CENTER:
    case LEFT_BOTTOM:
      x += w - (images_->left_arrow.width() - arrow_size);
      break;

    case RIGHT_TOP:
    case RIGHT_CENTER:
    case RIGHT_BOTTOM:
      x += images_->right_arrow.width() - arrow_size - border_size.width();
      break;

    case TOP_CENTER:
    case BOTTOM_CENTER:
      x +=  w / 2 - arrow_offset;
      break;

    case NONE:
    case FLOAT:
      x += w / 2 - border_size.width() / 2;
      break;
  }

  // Calculate bubble y coordinate.
  switch (arrow_location_) {
    case TOP_LEFT:
    case TOP_CENTER:
    case TOP_RIGHT:
      y += h - (images_->top_arrow.height() - arrow_size);
      break;

    case BOTTOM_LEFT:
    case BOTTOM_CENTER:
    case BOTTOM_RIGHT:
      y += images_->bottom_arrow.height() - arrow_size -
          border_size.height();
      break;

    case LEFT_TOP:
    case RIGHT_TOP:
      y += alignment_ == ALIGN_ARROW_TO_MID_ANCHOR ? h / 2 - arrow_offset :
           -(images_->top.height() - kBorderStrokeSize);
      break;

    case LEFT_BOTTOM:
    case RIGHT_BOTTOM:
      y += alignment_ == ALIGN_ARROW_TO_MID_ANCHOR ?
          h / 2 + arrow_offset - border_size.height() + 1 :
          h - border_size.height() + images_->bottom.height() -
              kBorderStrokeSize;
      break;

    case LEFT_CENTER:
    case RIGHT_CENTER:
      y += h / 2 - arrow_offset;
      break;

    case NONE:
      y += h;
      break;

    case FLOAT:
      y += h / 2 - border_size.height() / 2;
      break;
  }

  return gfx::Rect(x, y, border_size.width(), border_size.height());
}

gfx::Insets BubbleBorder::GetInsets() const {
  return GetInsetsForArrowLocation(arrow_location());
}

gfx::Insets BubbleBorder::GetInsetsForArrowLocation(
    ArrowLocation arrow_loc) const {
  int top = images_->top.height();
  int bottom = images_->bottom.height();
  int left = images_->left.width();
  int right = images_->right.width();
  switch (arrow_loc) {
    case TOP_LEFT:
    case TOP_CENTER:
    case TOP_RIGHT:
      top = std::max(top, images_->top_arrow.height());
      break;

    case BOTTOM_LEFT:
    case BOTTOM_CENTER:
    case BOTTOM_RIGHT:
      bottom = std::max(bottom, images_->bottom_arrow.height());
      break;

    case LEFT_TOP:
    case LEFT_CENTER:
    case LEFT_BOTTOM:
      left = std::max(left, images_->left_arrow.width());
      break;

    case RIGHT_TOP:
    case RIGHT_CENTER:
    case RIGHT_BOTTOM:
      right = std::max(right, images_->right_arrow.width());
      break;

    case NONE:
    case FLOAT:
      // Nothing to do.
      break;
  }
  return gfx::Insets(top, left, bottom, right);
}

int BubbleBorder::GetBorderThickness() const {
  return images_->border_thickness;
}

int BubbleBorder::GetBorderCornerRadius() const {
  return images_->corner_radius;
}

int BubbleBorder::GetArrowOffset(const gfx::Size& border_size) const {
  int arrow_offset = arrow_offset_;
  if (override_arrow_offset_) {
    arrow_offset = override_arrow_offset_;
  } else if (is_arrow_at_center(arrow_location_)) {
    if (is_arrow_on_horizontal(arrow_location_))
      arrow_offset = border_size.width() / 2;
    else
      arrow_offset = border_size.height() / 2;
  }

  // |arrow_offset_| contains the minimum offset required to draw border images
  // correctly. It's defined in terms of number of pixels from the beginning of
  // the edge. The maximum arrow offset is the edge size - |arrow_offset_|. The
  // following statement clamps the calculated arrow offset within that range.
  return std::max(arrow_offset_,
      std::min(arrow_offset, (is_arrow_on_horizontal(arrow_location_) ?
          border_size.width() : border_size.height()) - arrow_offset_));
}

// static
BubbleBorder::BorderImages* BubbleBorder::GetBorderImages(Shadow shadow) {
  CHECK_LT(shadow, SHADOW_COUNT);

  struct BorderImages*& images = border_images_[shadow];
  if (images)
    return images;

  switch (shadow) {
    case SHADOW:
      images = new BorderImages(kShadowImages,
                                arraysize(kShadowImages),
                                0, 10, 4);
      break;
    case NO_SHADOW:
      images = new BorderImages(kNoShadowImages,
                                arraysize(kNoShadowImages),
                                7, 0, 4);
      break;
    case BIG_SHADOW:
      images = new BorderImages(kBigShadowImages,
                                arraysize(kBigShadowImages),
                                9, 0, 3);
      break;
    case SMALL_SHADOW:
      images = new BorderImages(kSmallShadowImages,
                                arraysize(kSmallShadowImages),
                                6, 0, 3);
      break;
    case SHADOW_COUNT:
      NOTREACHED();
      break;
  }

  return images;
}

BubbleBorder::~BubbleBorder() {}

void BubbleBorder::Paint(const views::View& view, gfx::Canvas* canvas) {
  // Convenience shorthand variables.
  const int tl_width = images_->top_left.width();
  const int tl_height = images_->top_left.height();
  const int t_height = images_->top.height();
  const int tr_width = images_->top_right.width();
  const int tr_height = images_->top_right.height();
  const int l_width = images_->left.width();
  const int r_width = images_->right.width();
  const int br_width = images_->bottom_right.width();
  const int br_height = images_->bottom_right.height();
  const int b_height = images_->bottom.height();
  const int bl_width = images_->bottom_left.width();
  const int bl_height = images_->bottom_left.height();

  gfx::Insets insets = GetInsets();
  const int top = insets.top() - t_height;
  const int bottom = view.height() - insets.bottom() + b_height;
  const int left = insets.left() - l_width;
  const int right = view.width() - insets.right() + r_width;
  const int height = bottom - top;
  const int width = right - left;

  const ArrowLocation arrow_location = paint_arrow_ ? arrow_location_ : NONE;

  // |arrow_offset| is offset of arrow from the beginning of the edge.
  int arrow_offset = GetArrowOffset(view.size());
  if (!is_arrow_at_center(arrow_location)) {
    if (is_arrow_on_horizontal(arrow_location) &&
        !is_arrow_on_left(arrow_location)) {
      arrow_offset = view.width() - arrow_offset - 1;
    } else if (!is_arrow_on_horizontal(arrow_location) &&
               !is_arrow_on_top(arrow_location)) {
      arrow_offset = view.height() - arrow_offset - 1;
    }
  }

  // Left edge.
  if (arrow_location == LEFT_TOP ||
      arrow_location == LEFT_CENTER ||
      arrow_location == LEFT_BOTTOM) {
    int start_y = top + tl_height;
    int before_arrow =
        arrow_offset - start_y - images_->left_arrow.height() / 2;
    int after_arrow = height - tl_height - bl_height -
        images_->left_arrow.height() - before_arrow;
    // Shift tip coordinates half pixel so that skia draws the tip correctly.
    DrawArrowInterior(canvas,
        images_->left_arrow.width() - images_->arrow_interior_height - 0.5f,
        start_y + before_arrow + images_->left_arrow.height() / 2 - 0.5f);
    DrawEdgeWithArrow(canvas,
                      false,
                      images_->left,
                      images_->left_arrow,
                      left,
                      start_y,
                      before_arrow,
                      after_arrow,
                      images_->left.width() - images_->left_arrow.width());
  } else {
    canvas->TileImageInt(images_->left, left, top + tl_height, l_width,
                         height - tl_height - bl_height);
  }

  // Top left corner.
  canvas->DrawImageInt(images_->top_left, left, top);

  // Top edge.
  if (arrow_location == TOP_LEFT ||
      arrow_location == TOP_CENTER ||
      arrow_location == TOP_RIGHT) {
    int start_x = left + tl_width;
    int before_arrow = arrow_offset - start_x - images_->top_arrow.width() / 2;
    int after_arrow = width - tl_width - tr_width -
        images_->top_arrow.width() - before_arrow;
    DrawArrowInterior(canvas,
        start_x + before_arrow + images_->top_arrow.width() / 2,
        images_->top_arrow.height() - images_->arrow_interior_height);
    DrawEdgeWithArrow(canvas,
                      true,
                      images_->top,
                      images_->top_arrow,
                      start_x,
                      top,
                      before_arrow,
                      after_arrow,
                      images_->top.height() - images_->top_arrow.height());
  } else {
    canvas->TileImageInt(images_->top, left + tl_width, top,
                         width - tl_width - tr_width, t_height);
  }

  // Top right corner.
  canvas->DrawImageInt(images_->top_right, right - tr_width, top);

  // Right edge.
  if (arrow_location == RIGHT_TOP ||
      arrow_location == RIGHT_CENTER ||
      arrow_location == RIGHT_BOTTOM) {
    int start_y = top + tr_height;
    int before_arrow =
        arrow_offset - start_y - images_->right_arrow.height() / 2;
    int after_arrow = height - tl_height - bl_height -
        images_->right_arrow.height() - before_arrow;
    // Shift tip coordinates half pixel so that skia draws the tip correctly.
    DrawArrowInterior(canvas,
        right - r_width + images_->arrow_interior_height - 0.5f,
        start_y + before_arrow + images_->right_arrow.height() / 2 - 0.5f);
    DrawEdgeWithArrow(canvas,
                      false,
                      images_->right,
                      images_->right_arrow,
                      right - r_width,
                      start_y,
                      before_arrow,
                      after_arrow,
                      0);
  } else {
    canvas->TileImageInt(images_->right, right - r_width, top + tr_height,
                         r_width, height - tr_height - br_height);
  }

  // Bottom right corner.
  canvas->DrawImageInt(images_->bottom_right,
                       right - br_width,
                       bottom - br_height);

  // Bottom edge.
  if (arrow_location == BOTTOM_LEFT ||
      arrow_location == BOTTOM_CENTER ||
      arrow_location == BOTTOM_RIGHT) {
    int start_x = left + bl_width;
    int before_arrow =
        arrow_offset - start_x - images_->bottom_arrow.width() / 2;
    int after_arrow = width - bl_width - br_width -
        images_->bottom_arrow.width() - before_arrow;
    DrawArrowInterior(canvas,
        start_x + before_arrow + images_->bottom_arrow.width() / 2,
        bottom - b_height + images_->arrow_interior_height);
    DrawEdgeWithArrow(canvas,
                      true,
                      images_->bottom,
                      images_->bottom_arrow,
                      start_x,
                      bottom - b_height,
                      before_arrow,
                      after_arrow,
                      0);
  } else {
    canvas->TileImageInt(images_->bottom, left + bl_width, bottom - b_height,
                         width - bl_width - br_width, b_height);
  }

  // Bottom left corner.
  canvas->DrawImageInt(images_->bottom_left, left, bottom - bl_height);
}

void BubbleBorder::DrawEdgeWithArrow(gfx::Canvas* canvas,
                                     bool is_horizontal,
                                     const gfx::ImageSkia& edge,
                                     const gfx::ImageSkia& arrow,
                                     int start_x,
                                     int start_y,
                                     int before_arrow,
                                     int after_arrow,
                                     int offset) const {
  /* Here's what the parameters mean:
   *                     start_x
   *                       .
   *                       .        ┌───┐                 ┬ offset
   * start_y..........┌────┬────────┤ ▲ ├────────┬────┐
   *                  │  / │--------│∙ ∙│--------│ \  │
   *                  │ /  ├────────┴───┴────────┤  \ │
   *                  ├───┬┘                     └┬───┤
   *                       └───┬────┘   └───┬────┘
   *             before_arrow ─┘            └─ after_arrow
   */
  if (before_arrow) {
    canvas->TileImageInt(edge, start_x, start_y,
        is_horizontal ? before_arrow : edge.width(),
        is_horizontal ? edge.height() : before_arrow);
  }

  canvas->DrawImageInt(arrow,
      start_x + (is_horizontal ? before_arrow : offset),
      start_y + (is_horizontal ? offset : before_arrow));

  if (after_arrow) {
    start_x += (is_horizontal ? before_arrow + arrow.width() : 0);
    start_y += (is_horizontal ? 0 : before_arrow + arrow.height());
    canvas->TileImageInt(edge, start_x, start_y,
        is_horizontal ? after_arrow : edge.width(),
        is_horizontal ? edge.height() : after_arrow);
  }
}

void BubbleBorder::DrawArrowInterior(gfx::Canvas* canvas,
                                     float tip_x,
                                     float tip_y) const {
  const bool is_horizontal = is_arrow_on_horizontal(arrow_location_);
  const bool positive_offset = is_horizontal ?
      is_arrow_on_top(arrow_location_) : is_arrow_on_left(arrow_location_);
  const int offset_to_next_vertex = positive_offset ?
      images_->arrow_interior_height : -images_->arrow_interior_height;

  SkPath path;
  path.incReserve(4);
  path.moveTo(SkDoubleToScalar(tip_x), SkDoubleToScalar(tip_y));
  path.lineTo(SkDoubleToScalar(tip_x + offset_to_next_vertex),
              SkDoubleToScalar(tip_y + offset_to_next_vertex));
  if (is_horizontal) {
    path.lineTo(SkDoubleToScalar(tip_x - offset_to_next_vertex),
                SkDoubleToScalar(tip_y + offset_to_next_vertex));
  } else {
    path.lineTo(SkDoubleToScalar(tip_x + offset_to_next_vertex),
                SkDoubleToScalar(tip_y - offset_to_next_vertex));
  }
  path.close();

  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(background_color_);

  canvas->DrawPath(path, paint);
}

/////////////////////////

void BubbleBackground::Paint(gfx::Canvas* canvas, views::View* view) const {
  // Clip out the client bounds to prevent overlapping transparent widgets.
  if (!border_->client_bounds().IsEmpty()) {
    SkRect client_rect(gfx::RectToSkRect(border_->client_bounds()));
    canvas->sk_canvas()->clipRect(client_rect, SkRegion::kDifference_Op);
  }

  // The border of this view creates an anti-aliased round-rect region for the
  // contents, which we need to fill with the background color.
  // NOTE: This doesn't handle an arrow location of "NONE", which has square top
  // corners.
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(border_->background_color());
  SkPath path;
  gfx::Rect bounds(view->GetContentsBounds());
  bounds.Inset(-border_->GetBorderThickness(), -border_->GetBorderThickness());
  SkScalar radius = SkIntToScalar(border_->GetBorderCornerRadius());
  path.addRoundRect(gfx::RectToSkRect(bounds), radius, radius);
  canvas->DrawPath(path, paint);
}

}  // namespace views
