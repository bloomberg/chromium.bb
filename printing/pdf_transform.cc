// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/pdf_transform.h"

#include <algorithm>

#include "base/logging.h"
#include "ui/gfx/geometry/rect.h"

namespace printing {

double CalculateScaleFactor(const gfx::Rect& content_rect,
                            double src_width,
                            double src_height,
                            bool rotated) {
  if (src_width == 0 || src_height == 0)
    return 1.0;

  double actual_source_page_width = rotated ? src_height : src_width;
  double actual_source_page_height = rotated ? src_width : src_height;
  double ratio_x = static_cast<double>(content_rect.width()) /
                   actual_source_page_width;
  double ratio_y = static_cast<double>(content_rect.height()) /
                   actual_source_page_height;
  return std::min(ratio_x, ratio_y);
}

void SetDefaultClipBox(bool rotated, ClipBox* clip_box) {
  const int kDpi = 72;
  const float kPaperWidth = 8.5 * kDpi;
  const float kPaperHeight = 11 * kDpi;
  clip_box->left = 0;
  clip_box->bottom = 0;
  clip_box->right = rotated ? kPaperHeight : kPaperWidth;
  clip_box->top = rotated ? kPaperWidth : kPaperHeight;
}

void CalculateMediaBoxAndCropBox(bool rotated,
                                 bool has_media_box,
                                 bool has_crop_box,
                                 printing::ClipBox* media_box,
                                 printing::ClipBox* crop_box) {
  if (!has_media_box && !has_crop_box) {
    SetDefaultClipBox(rotated, crop_box);
    SetDefaultClipBox(rotated, media_box);
  } else if (has_crop_box && !has_media_box) {
    *media_box = *crop_box;
  } else if (has_media_box && !has_crop_box) {
    *crop_box = *media_box;
  }
}

ClipBox CalculateClipBoxBoundary(const ClipBox& media_box,
                                 const ClipBox& crop_box) {
  ClipBox clip_box;

  // Clip |media_box| to the size of |crop_box|, but ignore |crop_box| if it is
  // bigger than |media_box|.
  clip_box.left =
      (crop_box.left < media_box.left) ? media_box.left : crop_box.left;
  clip_box.right =
      (crop_box.right > media_box.right) ? media_box.right : crop_box.right;
  clip_box.top = (crop_box.top > media_box.top) ? media_box.top : crop_box.top;
  clip_box.bottom =
      (crop_box.bottom < media_box.bottom) ? media_box.bottom : crop_box.bottom;
  return clip_box;
}

void ScaleClipBox(double scale_factor, ClipBox* box) {
  box->left *= scale_factor;
  box->right *= scale_factor;
  box->bottom *= scale_factor;
  box->top *= scale_factor;
}

void CalculateScaledClipBoxOffset(const gfx::Rect& content_rect,
                                  const ClipBox& source_clip_box,
                                  double* offset_x,
                                  double* offset_y) {
  const float clip_box_width = source_clip_box.right - source_clip_box.left;
  const float clip_box_height = source_clip_box.top - source_clip_box.bottom;

  // Center the intended clip region to real clip region.
  *offset_x = (content_rect.width() - clip_box_width) / 2 + content_rect.x() -
              source_clip_box.left;
  *offset_y = (content_rect.height() - clip_box_height) / 2 + content_rect.y() -
              source_clip_box.bottom;
}

void CalculateNonScaledClipBoxOffset(const gfx::Rect& content_rect,
                                     int rotation,
                                     int page_width,
                                     int page_height,
                                     const ClipBox& source_clip_box,
                                     double* offset_x,
                                     double* offset_y) {
  // Align the intended clip region to left-top corner of real clip region.
  switch (rotation) {
    case 0:
      *offset_x = -1 * source_clip_box.left;
      *offset_y = page_height - source_clip_box.top;
      break;
    case 1:
      *offset_x = 0;
      *offset_y = -1 * source_clip_box.bottom;
      break;
    case 2:
      *offset_x = page_width - source_clip_box.right;
      *offset_y = 0;
      break;
    case 3:
      *offset_x = page_height - source_clip_box.right;
      *offset_y = page_width - source_clip_box.top;
      break;
    default:
      NOTREACHED();
      break;
  }
}

}  // namespace printing
