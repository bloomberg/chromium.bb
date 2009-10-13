// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file implements a simple generic version of the WebKitThemeEngine,
// which is used to draw all the native controls on a web page. We use this
// file when running in layout test mode in order to remove any
// platform-specific rendering differences due to themes, colors, etc.
//

#include "webkit/tools/test_shell/test_shell_webthemecontrol.h"

#include "base/logging.h"
#include "skia/ext/platform_canvas.h"
#include "skia/ext/skia_utils_win.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"

namespace TestShellWebTheme {

const SkColor kEdgeColor     = SK_ColorBLACK;
const SkColor kReadOnlyColor = SkColorSetRGB(0xe9, 0xc2, 0xa6);
const SkColor kFgColor       = SK_ColorBLACK;

const SkColor kBgColors[] = {
  SK_ColorBLACK,                    // Unknown
  SkColorSetRGB(0xc9, 0xc9, 0xc9),  // Disabled
  SkColorSetRGB(0xf3, 0xe0, 0xd0),  // Readonly
  SkColorSetRGB(0x89, 0xc4, 0xff),  // Normal
  SkColorSetRGB(0x43, 0xf9, 0xff),  // Hot
  SkColorSetRGB(0x20, 0xf6, 0xcc),  // Focused
  SkColorSetRGB(0x00, 0xf3, 0xac),  // Hover
  SkColorSetRGB(0xa9, 0xff, 0x12)   // Pressed
};

Control::Control(skia::PlatformCanvas *canvas, const SkIRect &irect,
                 Type ctype, State cstate)
    : canvas_(canvas),
      irect_(irect),
      type_(ctype),
      state_(cstate),
      left_(irect.fLeft),
      right_(irect.fRight),
      top_(irect.fTop),
      bottom_(irect.fBottom),
      height_(irect.height()),
      width_(irect.width()),
      edge_color_(kEdgeColor),
      bg_color_(kBgColors[cstate]),
      fg_color_(kFgColor) {
}

Control::~Control() {
}

void Control::box(const SkIRect &rect, SkColor fill_color) {
  SkPaint paint;

  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(fill_color);
  canvas_->drawIRect(rect, paint);

  paint.setColor(edge_color_);
  paint.setStyle(SkPaint::kStroke_Style);
  canvas_->drawIRect(rect, paint);
}

void Control::line(int x0, int y0, int x1, int y1, SkColor color) {
  SkPaint paint;
  paint.setColor(color);
  canvas_->drawLine(SkIntToScalar(x0), SkIntToScalar(y0),
                    SkIntToScalar(x1), SkIntToScalar(y1),
                    paint);
}

void Control::triangle(int x0, int y0,
                       int x1, int y1,
                       int x2, int y2,
                       SkColor color) {
  SkPath path;
  SkPaint paint;

  paint.setColor(color);
  paint.setStyle(SkPaint::kFill_Style);
  path.incReserve(4);
  path.moveTo(SkIntToScalar(x0), SkIntToScalar(y0));
  path.lineTo(SkIntToScalar(x1), SkIntToScalar(y1));
  path.lineTo(SkIntToScalar(x2), SkIntToScalar(y2));
  path.close();
  canvas_->drawPath(path, paint);

  paint.setColor(edge_color_);
  paint.setStyle(SkPaint::kStroke_Style);
  canvas_->drawPath(path, paint);
}

void Control::roundRect(SkColor color) {
  SkRect rect;
  SkScalar radius = SkIntToScalar(5);
  SkPaint paint;

  rect.set(irect_);
  paint.setColor(color);
  paint.setStyle(SkPaint::kFill_Style);
  canvas_->drawRoundRect(rect, radius, radius, paint);

  paint.setColor(edge_color_);
  paint.setStyle(SkPaint::kStroke_Style);
  canvas_->drawRoundRect(rect, radius, radius, paint);
}

void Control::oval(SkColor color) {
  SkRect rect;
  SkPaint paint;

  rect.set(irect_);
  paint.setColor(color);
  paint.setStyle(SkPaint::kFill_Style);
  canvas_->drawOval(rect, paint);

  paint.setColor(edge_color_);
  paint.setStyle(SkPaint::kStroke_Style);
  canvas_->drawOval(rect, paint);
}

void Control::circle(SkScalar radius, SkColor color) {
  SkScalar cy = SkIntToScalar(top_  + height_ / 2);
  SkScalar cx = SkIntToScalar(left_ + width_ / 2);
  SkPaint paint;

  paint.setColor(color);
  paint.setStyle(SkPaint::kFill_Style);
  canvas_->drawCircle(cx, cy, radius, paint);

  paint.setColor(edge_color_);
  paint.setStyle(SkPaint::kStroke_Style);
  canvas_->drawCircle(cx, cy, radius, paint);
}

void Control::nested_boxes(int indent_left, int indent_top,
                           int indent_right, int indent_bottom,
                           SkColor outer_color, SkColor inner_color) {
  SkIRect lirect;
  box(irect_, outer_color);
  lirect.set(irect_.fLeft + indent_left,   irect_.fTop + indent_top,
             irect_.fRight - indent_right, irect_.fBottom - indent_bottom);
  box(lirect, inner_color);
}


void Control::markState() {
  // The horizontal lines in a read only control are spaced by this amount.
  const int kReadOnlyLineOffset = 5;

  // The length of a triangle side for the corner marks.
  const int kTriangleSize = 5;

  switch (state_) {
    case kUnknown_State:   // FALLTHROUGH
    case kDisabled_State:  // FALLTHROUGH
    case kNormal_State:
      // Don't visually mark these states (color is enough).
      break;
    case kReadOnly_State:
      // Drawing lines across the control.
      for (int i = top_ + kReadOnlyLineOffset; i < bottom_ ;
           i += kReadOnlyLineOffset) {
        line(left_ + 1, i, right_ - 1, i, kReadOnlyColor);
      }
      break;
    case kHot_State:
      // Draw a triangle in the upper left corner of the control.
      triangle(left_,                  top_,
               left_ + kTriangleSize,  top_,
               left_,                  top_ + kTriangleSize,    edge_color_);
      break;
    case kHover_State:
      // Draw a triangle in the upper right corner of the control.
      triangle(right_,                 top_,
               right_,                 top_ + kTriangleSize,
               right_ - kTriangleSize, top_,                    edge_color_);
      break;
    case kFocused_State:
      // Draw a triangle in the bottom right corner of the control.
      triangle(right_,                 bottom_,
               right_ - kTriangleSize, bottom_,
               right_,                 bottom_ - kTriangleSize, edge_color_);
         break;
    case kPressed_State:
      // Draw a triangle in the bottom left corner of the control.
      triangle(left_,                 bottom_,
               left_,                 bottom_ - kTriangleSize,
               left_ + kTriangleSize, bottom_,                  edge_color_);
      break;
    default:
      NOTREACHED();
      break;
  }
}

void Control::draw() {
  int half_width = width_ / 2;
  int half_height = height_ / 2;
  int quarter_width = width_ / 4;
  int quarter_height = height_ / 4;

  // Indent amounts for the check in a checkbox or radio button.
  const int kCheckIndent = 3;

  // Indent amounts for short and long sides of the scrollbar notches.
  const int kNotchLongOffset = 1;
  const int kNotchShortOffset = 4;
  const int kNoOffset = 0;
  int short_offset;
  int long_offset;

  // Indent amounts for the short and long sides of a scroll thumb box.
  const int kThumbLongIndent = 0;
  const int kThumbShortIndent = 2;

  // Indents for the crosshatch on a scroll grip.
  const int kGripLongIndent = 3;
  const int kGripShortIndent = 5;

  // Indents for the the slider track.
  const int kSliderIndent = 2;

  canvas_->beginPlatformPaint();
  switch (type_) {
    case kUnknown_Type:
      NOTREACHED();
      break;
    case kTextField_Type:
      // We render this by hand outside of this function.
      NOTREACHED();
      break;
    case kPushButton_Type:
      // push buttons render as a rounded rectangle
      roundRect(bg_color_);
      break;
    case kUncheckedBox_Type:
      // Unchecked boxes are simply plain boxes.
      box(irect_, bg_color_);
      break;
    case kCheckedBox_Type:
      nested_boxes(kCheckIndent, kCheckIndent, kCheckIndent, kCheckIndent,
                   bg_color_, fg_color_);
      break;
    case kUncheckedRadio_Type:
      circle(SkIntToScalar(half_height), bg_color_);
      break;
    case kCheckedRadio_Type:
      circle(SkIntToScalar(half_height), bg_color_);
      circle(SkIntToScalar(half_height - kCheckIndent), fg_color_);
      break;
    case kHorizontalScrollTrackBack_Type:
      // Draw a box with a notch at the left.
      long_offset = half_height - kNotchLongOffset;
      short_offset = width_ - kNotchShortOffset;
      nested_boxes(kNoOffset, long_offset, short_offset, long_offset,
                   bg_color_, edge_color_);
      break;
    case kHorizontalScrollTrackForward_Type:
      // Draw a box with a notch at the right.
      long_offset  = half_height - kNotchLongOffset;
      short_offset = width_      - kNotchShortOffset;
      nested_boxes(short_offset, long_offset, kNoOffset, long_offset,
                   bg_color_, fg_color_);
      break;
    case kVerticalScrollTrackBack_Type:
      // Draw a box with a notch at the top.
      long_offset  = half_width - kNotchLongOffset;
      short_offset = height_    - kNotchShortOffset;
      nested_boxes(long_offset, kNoOffset, long_offset, short_offset,
                   bg_color_, fg_color_);
      break;
    case kVerticalScrollTrackForward_Type:
      // Draw a box with a notch at the bottom.
      long_offset  = half_width - kNotchLongOffset;
      short_offset = height_    - kNotchShortOffset;
      nested_boxes(long_offset, short_offset, long_offset, kNoOffset,
                   bg_color_, fg_color_);
      break;
    case kHorizontalScrollThumb_Type:
      // Draw a narrower box on top of the outside box.
      nested_boxes(kThumbLongIndent, kThumbShortIndent, kThumbLongIndent,
                   kThumbShortIndent, bg_color_, bg_color_);
      break;
    case kVerticalScrollThumb_Type:
      // Draw a shorter box on top of the outside box.
      nested_boxes(kThumbShortIndent, kThumbLongIndent, kThumbShortIndent,
                   kThumbLongIndent, bg_color_, bg_color_);
      break;
    case kHorizontalSliderThumb_Type:
      // Slider thumbs are ovals.
      oval(bg_color_);
      break;
    case kHorizontalScrollGrip_Type:
      // Draw a horizontal crosshatch for the grip.
      long_offset = half_width - kGripLongIndent;
      line(left_  + kGripLongIndent,  top_    + half_height,
           right_ - kGripLongIndent,  top_    + half_height,      fg_color_);
      line(left_  + long_offset,      top_    + kGripShortIndent,
           left_  + long_offset,      bottom_ - kGripShortIndent, fg_color_);
      line(right_ - long_offset,      top_    + kGripShortIndent,
           right_ - long_offset,      bottom_ - kGripShortIndent, fg_color_);
       break;
    case kVerticalScrollGrip_Type:
      // Draw a vertical crosshatch for the grip.
      long_offset = half_height - kGripLongIndent;
      line(left_ + half_width,        top_    + kGripLongIndent,
           left_ + half_width,        bottom_ - kGripLongIndent, fg_color_);
      line(left_ + kGripShortIndent,  top_    + long_offset,
           right_ - kGripShortIndent, top_    + long_offset,     fg_color_);
      line(left_ + kGripShortIndent,  bottom_ - long_offset,
           right_ - kGripShortIndent, bottom_ - long_offset,     fg_color_);
      break;
    case kLeftArrow_Type:
      // Draw a left arrow inside a box.
      box(irect_, bg_color_);
      triangle(right_ - quarter_width, top_    + quarter_height,
               right_ - quarter_width, bottom_ - quarter_height,
               left_  + quarter_width, top_    + half_height,    fg_color_);
      break;
    case kRightArrow_Type:
      // Draw a left arrow inside a box.
      box(irect_, bg_color_);
      triangle(left_  + quarter_width, top_    + quarter_height,
               right_ - quarter_width, top_    + half_height,
               left_  + quarter_width, bottom_ - quarter_height, fg_color_);
      break;
    case kUpArrow_Type:
      // Draw an up arrow inside a box.
      box(irect_, bg_color_);
      triangle(left_  + quarter_width, bottom_ - quarter_height,
               left_  + half_width,    top_    + quarter_height,
               right_ - quarter_width, bottom_ - quarter_height, fg_color_);
      break;
    case kDownArrow_Type:
      // Draw a down arrow inside a box.
      box(irect_, bg_color_);
      triangle(left_  + quarter_width, top_    + quarter_height,
               right_ - quarter_width, top_    + quarter_height,
               left_  + half_width,    bottom_ - quarter_height, fg_color_);
      break;
    case kHorizontalSliderTrack_Type:
      // Draw a narrow rect for the track plus box hatches on the ends.
      SkIRect lirect;
      lirect = irect_;
      lirect.inset(kNoOffset, half_height - kSliderIndent);
      box(lirect, bg_color_);
      line(left_,  top_, left_,  bottom_, edge_color_);
      line(right_, top_, right_, bottom_, edge_color_);
      break;
    case kDropDownButton_Type:
      // Draw a box with a big down arrow on top.
      box(irect_, bg_color_);
      triangle(left_  + quarter_width, top_,
               right_ - quarter_width, top_,
               left_  + half_width,    bottom_, fg_color_);
      break;
    default:
      NOTREACHED();
      break;
  }

  markState();
  canvas_->endPlatformPaint();
}

// Because rendering a text field is dependent on input
// parameters the other controls don't have, we render it directly
// rather than trying to overcomplicate draw() further.
void Control::drawTextField(bool draw_edges, bool fill_content_area,
                            SkColor color) {
  SkPaint paint;

  canvas_->beginPlatformPaint();
  if (fill_content_area) {
    paint.setColor(color);
    paint.setStyle(SkPaint::kFill_Style);
    canvas_->drawIRect(irect_, paint);
  }
  if (draw_edges) {
    paint.setColor(edge_color_);
    paint.setStyle(SkPaint::kStroke_Style);
    canvas_->drawIRect(irect_, paint);
  }

  markState();
  canvas_->endPlatformPaint();
}

}  // namespace TestShellWebTheme

