// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/bubble/bubble_border.h"

#include <algorithm>  // for std::max

#include "base/logging.h"
#include "grit/ui_resources.h"
#include "grit/ui_resources_standard.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/path.h"

namespace views {

struct BubbleBorder::BorderImages {
  BorderImages()
      : left(NULL),
        top_left(NULL),
        top(NULL),
        top_right(NULL),
        right(NULL),
        bottom_right(NULL),
        bottom(NULL),
        bottom_left(NULL),
        left_arrow(NULL),
        top_arrow(NULL),
        right_arrow(NULL),
        bottom_arrow(NULL) {
  }

  SkBitmap* left;
  SkBitmap* top_left;
  SkBitmap* top;
  SkBitmap* top_right;
  SkBitmap* right;
  SkBitmap* bottom_right;
  SkBitmap* bottom;
  SkBitmap* bottom_left;
  SkBitmap* left_arrow;
  SkBitmap* top_arrow;
  SkBitmap* right_arrow;
  SkBitmap* bottom_arrow;
};

// static
struct BubbleBorder::BorderImages* BubbleBorder::normal_images_ = NULL;
struct BubbleBorder::BorderImages* BubbleBorder::shadow_images_ = NULL;


// The height inside the arrow image, in pixels.
static const int kArrowInteriorHeight = 7;

BubbleBorder::BubbleBorder(ArrowLocation arrow_location, Shadow shadow)
    : override_arrow_offset_(0),
      arrow_location_(arrow_location),
      alignment_(ALIGN_ARROW_TO_MID_ANCHOR),
      background_color_(SK_ColorWHITE) {
  images_ = GetBorderImages(shadow);

  // Calculate horizontal and vertical insets for arrow by ensuring that
  // the widest arrow and corner images will have enough room to avoid overlap
  int offset_x =
      (std::max(images_->top_arrow->width(),
                images_->bottom_arrow->width()) / 2) +
      std::max(std::max(images_->top_left->width(),
                        images_->top_right->width()),
               std::max(images_->bottom_left->width(),
                        images_->bottom_right->width()));
  int offset_y =
      (std::max(images_->left_arrow->height(),
                images_->right_arrow->height()) / 2) +
      std::max(std::max(images_->top_left->height(),
                        images_->top_right->height()),
               std::max(images_->bottom_left->height(),
                        images_->bottom_right->height()));
  arrow_offset_ = std::max(offset_x, offset_y);
}

gfx::Rect BubbleBorder::GetBounds(const gfx::Rect& position_relative_to,
                                  const gfx::Size& contents_size) const {
  // Desired size is size of contents enlarged by the size of the border images.
  gfx::Size border_size(contents_size);
  gfx::Insets insets;
  GetInsets(&insets);
  border_size.Enlarge(insets.left() + insets.right(),
                      insets.top() + insets.bottom());

  // Screen position depends on the arrow location.
  // The arrow should overlap the target by some amount since there is space
  // for shadow between arrow tip and bitmap bounds.
  const int kArrowOverlap = 3;
  int x = position_relative_to.x();
  int y = position_relative_to.y();
  int w = position_relative_to.width();
  int h = position_relative_to.height();
  int arrow_offset = override_arrow_offset_ ? override_arrow_offset_ :
                                              arrow_offset_;

  // Calculate bubble x coordinate.
  switch (arrow_location_) {
    case TOP_LEFT:
    case BOTTOM_LEFT:
      x += alignment_ == ALIGN_ARROW_TO_MID_ANCHOR ? w / 2 - arrow_offset :
           -kArrowOverlap;
      break;

    case TOP_RIGHT:
    case BOTTOM_RIGHT:
      x += alignment_ == ALIGN_ARROW_TO_MID_ANCHOR ?
          w / 2 + arrow_offset - border_size.width() + 1 :
          w - border_size.width() + kArrowOverlap;
      break;

    case LEFT_TOP:
    case LEFT_BOTTOM:
      x += w - kArrowOverlap;
      break;

    case RIGHT_TOP:
    case RIGHT_BOTTOM:
      x += kArrowOverlap - border_size.width();
      break;

    case NONE:
    case FLOAT:
      x += w / 2 - border_size.width() / 2;
      break;
  }

  // Calculate bubble y coordinate.
  switch (arrow_location_) {
    case TOP_LEFT:
    case TOP_RIGHT:
      y += h - kArrowOverlap;
      break;

    case BOTTOM_LEFT:
    case BOTTOM_RIGHT:
      y += kArrowOverlap - border_size.height();
      break;

    case LEFT_TOP:
    case RIGHT_TOP:
      y += alignment_ == ALIGN_ARROW_TO_MID_ANCHOR ? h / 2 - arrow_offset :
           -kArrowOverlap;
      break;

    case LEFT_BOTTOM:
    case RIGHT_BOTTOM:
      y += alignment_ == ALIGN_ARROW_TO_MID_ANCHOR ?
          h / 2 + arrow_offset - border_size.height() + 1 :
          h - border_size.height() + kArrowOverlap;
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

void BubbleBorder::GetInsets(gfx::Insets* insets) const {
  int top = images_->top->height();
  int bottom = images_->bottom->height();
  int left = images_->left->width();
  int right = images_->right->width();
  switch (arrow_location_) {
    case TOP_LEFT:
    case TOP_RIGHT:
      top = std::max(top, images_->top_arrow->height());
      break;

    case BOTTOM_LEFT:
    case BOTTOM_RIGHT:
      bottom = std::max(bottom, images_->bottom_arrow->height());
      break;

    case LEFT_TOP:
    case LEFT_BOTTOM:
      left = std::max(left, images_->left_arrow->width());
      break;

    case RIGHT_TOP:
    case RIGHT_BOTTOM:
      right = std::max(right, images_->right_arrow->width());
      break;

    case NONE:
    case FLOAT:
      // Nothing to do.
      break;
  }
  insets->Set(top, left, bottom, right);
}

int BubbleBorder::SetArrowOffset(int offset, const gfx::Size& contents_size) {
  gfx::Size border_size(contents_size);
  gfx::Insets insets;
  GetInsets(&insets);
  border_size.Enlarge(insets.left() + insets.right(),
                      insets.top() + insets.bottom());
  offset = std::max(arrow_offset_,
      std::min(offset, (is_arrow_on_horizontal(arrow_location_) ?
          border_size.width() : border_size.height()) - arrow_offset_));
  override_arrow_offset_ = offset;
  return override_arrow_offset_;
}

// static
BubbleBorder::BorderImages* BubbleBorder::GetBorderImages(Shadow shadow) {
  if (shadow == SHADOW && shadow_images_ == NULL) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    shadow_images_ = new BorderImages();
    shadow_images_->left = rb.GetBitmapNamed(IDR_BUBBLE_SHADOW_L);
    shadow_images_->top_left = rb.GetBitmapNamed(IDR_BUBBLE_SHADOW_TL);
    shadow_images_->top = rb.GetBitmapNamed(IDR_BUBBLE_SHADOW_T);
    shadow_images_->top_right = rb.GetBitmapNamed(IDR_BUBBLE_SHADOW_TR);
    shadow_images_->right = rb.GetBitmapNamed(IDR_BUBBLE_SHADOW_R);
    shadow_images_->bottom_right = rb.GetBitmapNamed(IDR_BUBBLE_SHADOW_BR);
    shadow_images_->bottom = rb.GetBitmapNamed(IDR_BUBBLE_SHADOW_B);
    shadow_images_->bottom_left = rb.GetBitmapNamed(IDR_BUBBLE_SHADOW_BL);
    shadow_images_->left_arrow = new SkBitmap();
    shadow_images_->top_arrow = new SkBitmap();
    shadow_images_->right_arrow = new SkBitmap();
    shadow_images_->bottom_arrow = new SkBitmap();
  } else if (shadow == NO_SHADOW && normal_images_ == NULL) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    normal_images_ = new BorderImages();
    normal_images_->left = rb.GetBitmapNamed(IDR_BUBBLE_L);
    normal_images_->top_left = rb.GetBitmapNamed(IDR_BUBBLE_TL);
    normal_images_->top = rb.GetBitmapNamed(IDR_BUBBLE_T);
    normal_images_->top_right = rb.GetBitmapNamed(IDR_BUBBLE_TR);
    normal_images_->right = rb.GetBitmapNamed(IDR_BUBBLE_R);
    normal_images_->bottom_right = rb.GetBitmapNamed(IDR_BUBBLE_BR);
    normal_images_->bottom = rb.GetBitmapNamed(IDR_BUBBLE_B);
    normal_images_->bottom_left = rb.GetBitmapNamed(IDR_BUBBLE_BL);
    normal_images_->left_arrow = rb.GetBitmapNamed(IDR_BUBBLE_L_ARROW);
    normal_images_->top_arrow = rb.GetBitmapNamed(IDR_BUBBLE_T_ARROW);
    normal_images_->right_arrow = rb.GetBitmapNamed(IDR_BUBBLE_R_ARROW);
    normal_images_->bottom_arrow = rb.GetBitmapNamed(IDR_BUBBLE_B_ARROW);
  }
  return shadow == SHADOW ? shadow_images_ : normal_images_;
}

BubbleBorder::~BubbleBorder() {}

void BubbleBorder::Paint(const views::View& view, gfx::Canvas* canvas) const {
  // Convenience shorthand variables.
  const int tl_width = images_->top_left->width();
  const int tl_height = images_->top_left->height();
  const int t_height = images_->top->height();
  const int tr_width = images_->top_right->width();
  const int tr_height = images_->top_right->height();
  const int l_width = images_->left->width();
  const int r_width = images_->right->width();
  const int br_width = images_->bottom_right->width();
  const int br_height = images_->bottom_right->height();
  const int b_height = images_->bottom->height();
  const int bl_width = images_->bottom_left->width();
  const int bl_height = images_->bottom_left->height();

  gfx::Insets insets;
  GetInsets(&insets);
  const int top = insets.top() - t_height;
  const int bottom = view.height() - insets.bottom() + b_height;
  const int left = insets.left() - l_width;
  const int right = view.width() - insets.right() + r_width;
  const int height = bottom - top;
  const int width = right - left;

  // |arrow_offset| is offset of arrow from the begining of the edge.
  int arrow_offset = arrow_offset_;
  if (override_arrow_offset_)
    arrow_offset = override_arrow_offset_;
  else if (is_arrow_on_horizontal(arrow_location_) &&
           !is_arrow_on_left(arrow_location_)) {
    arrow_offset = view.width() - arrow_offset - 1;
  } else if (!is_arrow_on_horizontal(arrow_location_) &&
             !is_arrow_on_top(arrow_location_)) {
    arrow_offset = view.height() - arrow_offset - 1;
  }

  // Left edge.
  if (arrow_location_ == LEFT_TOP || arrow_location_ == LEFT_BOTTOM) {
    int start_y = top + tl_height;
    int before_arrow =
        arrow_offset - start_y - images_->left_arrow->height() / 2;
    int after_arrow = height - tl_height - bl_height -
        images_->left_arrow->height() - before_arrow;
    int tip_y = start_y + before_arrow + images_->left_arrow->height() / 2;
    DrawArrowInterior(canvas,
                      false,
                      images_->left_arrow->width() - kArrowInteriorHeight,
                      tip_y,
                      kArrowInteriorHeight,
                      images_->left_arrow->height() / 2 - 1);
    DrawEdgeWithArrow(canvas,
                      false,
                      images_->left,
                      images_->left_arrow,
                      left,
                      start_y,
                      before_arrow,
                      after_arrow,
                      images_->left->width() - images_->left_arrow->width());
  } else {
    canvas->TileImageInt(*images_->left, left, top + tl_height, l_width,
                         height - tl_height - bl_height);
  }

  // Top left corner.
  canvas->DrawBitmapInt(*images_->top_left, left, top);

  // Top edge.
  if (arrow_location_ == TOP_LEFT || arrow_location_ == TOP_RIGHT) {
    int start_x = left + tl_width;
    int before_arrow = arrow_offset - start_x - images_->top_arrow->width() / 2;
    int after_arrow = width - tl_width - tr_width -
        images_->top_arrow->width() - before_arrow;
    DrawArrowInterior(canvas,
                      true,
                      start_x + before_arrow + images_->top_arrow->width() / 2,
                      images_->top_arrow->height() - kArrowInteriorHeight,
                      1 - images_->top_arrow->width() / 2,
                      kArrowInteriorHeight);
    DrawEdgeWithArrow(canvas,
                      true,
                      images_->top,
                      images_->top_arrow,
                      start_x,
                      top,
                      before_arrow,
                      after_arrow,
                      images_->top->height() - images_->top_arrow->height());
  } else {
    canvas->TileImageInt(*images_->top, left + tl_width, top,
                         width - tl_width - tr_width, t_height);
  }

  // Top right corner.
  canvas->DrawBitmapInt(*images_->top_right, right - tr_width, top);

  // Right edge.
  if (arrow_location_ == RIGHT_TOP || arrow_location_ == RIGHT_BOTTOM) {
    int start_y = top + tr_height;
    int before_arrow =
        arrow_offset - start_y - images_->right_arrow->height() / 2;
    int after_arrow = height - tl_height - bl_height -
        images_->right_arrow->height() - before_arrow;
    int tip_y = start_y + before_arrow + images_->right_arrow->height() / 2;
    DrawArrowInterior(canvas,
                      false,
                      right - r_width + kArrowInteriorHeight,
                      tip_y,
                      -kArrowInteriorHeight,
                      images_->right_arrow->height() / 2 - 1);
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
    canvas->TileImageInt(*images_->right, right - r_width, top + tr_height,
                         r_width, height - tr_height - br_height);
  }

  // Bottom right corner.
  canvas->DrawBitmapInt(*images_->bottom_right,
                        right - br_width,
                        bottom - br_height);

  // Bottom edge.
  if (arrow_location_ == BOTTOM_LEFT || arrow_location_ == BOTTOM_RIGHT) {
    int start_x = left + bl_width;
    int before_arrow =
        arrow_offset - start_x - images_->bottom_arrow->width() / 2;
    int after_arrow = width - bl_width - br_width -
        images_->bottom_arrow->width() - before_arrow;
    int tip_x = start_x + before_arrow + images_->bottom_arrow->width() / 2;
    DrawArrowInterior(canvas,
                      true,
                      tip_x,
                      bottom - b_height + kArrowInteriorHeight,
                      1 - images_->bottom_arrow->width() / 2,
                      -kArrowInteriorHeight);
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
    canvas->TileImageInt(*images_->bottom, left + bl_width, bottom - b_height,
                         width - bl_width - br_width, b_height);
  }

  // Bottom left corner.
  canvas->DrawBitmapInt(*images_->bottom_left, left, bottom - bl_height);
}

void BubbleBorder::DrawEdgeWithArrow(gfx::Canvas* canvas,
                                     bool is_horizontal,
                                     SkBitmap* edge,
                                     SkBitmap* arrow,
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
    canvas->TileImageInt(*edge, start_x, start_y,
        is_horizontal ? before_arrow : edge->width(),
        is_horizontal ? edge->height() : before_arrow);
  }

  canvas->DrawBitmapInt(*arrow,
      start_x + (is_horizontal ? before_arrow : offset),
      start_y + (is_horizontal ? offset : before_arrow));

  if (after_arrow) {
    start_x += (is_horizontal ? before_arrow + arrow->width() : 0);
    start_y += (is_horizontal ? 0 : before_arrow + arrow->height());
    canvas->TileImageInt(*edge, start_x, start_y,
        is_horizontal ? after_arrow : edge->width(),
        is_horizontal ? edge->height() : after_arrow);
  }
}

void BubbleBorder::DrawArrowInterior(gfx::Canvas* canvas,
                                     bool is_horizontal,
                                     int tip_x,
                                     int tip_y,
                                     int shift_x,
                                     int shift_y) const {
  /* This function fills the interior of the arrow with background color.
   * It draws isosceles triangle under semitransparent arrow tip.
   *
   * Here's what the parameters mean:
   *
   *    ┌──────── |tip_x|
   * ┌─────┐
   * │  ▲  │ ──── |tip y|
   * │∙∙∙∙∙│ ┐
   * └─────┘ └─── |shift_x| (offset from tip to vertexes of isosceles triangle)
   *  └────────── |shift_y|
   */
  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(background_color_);
  gfx::Path path;
  path.incReserve(4);
  path.moveTo(SkIntToScalar(tip_x), SkIntToScalar(tip_y));
  path.lineTo(SkIntToScalar(tip_x + shift_x),
              SkIntToScalar(tip_y + shift_y));
  if (is_horizontal)
    path.lineTo(SkIntToScalar(tip_x - shift_x), SkIntToScalar(tip_y + shift_y));
  else
    path.lineTo(SkIntToScalar(tip_x + shift_x), SkIntToScalar(tip_y - shift_y));
  path.close();
  canvas->GetSkCanvas()->drawPath(path, paint);
}

/////////////////////////

void BubbleBackground::Paint(gfx::Canvas* canvas, views::View* view) const {
  // The border of this view creates an anti-aliased round-rect region for the
  // contents, which we need to fill with the background color.
  // NOTE: This doesn't handle an arrow location of "NONE", which has square top
  // corners.
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(border_->background_color());
  gfx::Path path;
  gfx::Rect bounds(view->GetContentsBounds());
  SkRect rect;
  rect.set(SkIntToScalar(bounds.x()), SkIntToScalar(bounds.y()),
           SkIntToScalar(bounds.right()), SkIntToScalar(bounds.bottom()));
  SkScalar radius = SkIntToScalar(BubbleBorder::GetCornerRadius());
  path.addRoundRect(rect, radius, radius);
  canvas->GetSkCanvas()->drawPath(path, paint);
}

}  // namespace views
