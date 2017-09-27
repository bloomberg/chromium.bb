// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/paint_vector_icon.h"

#include <gtest/gtest.h>
#include <vector>

#include "base/i18n/rtl.h"
#include "cc/paint/paint_record.h"
#include "cc/paint/paint_recorder.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/vector_icon_types.h"

namespace gfx {

namespace {

class MockCanvas : public SkCanvas {
 public:
  MockCanvas(int width, int height) : SkCanvas(width, height) {}

  // SkCanvas overrides:
  void onDrawPath(const SkPath& path, const SkPaint& paint) override {
    paths_.push_back(path);
  }

  const std::vector<SkPath>& paths() const { return paths_; }

 private:
  std::vector<SkPath> paths_;

  DISALLOW_COPY_AND_ASSIGN(MockCanvas);
};

// Tests that a relative move to command (R_MOVE_TO) after a close command
// (CLOSE) uses the correct starting point. See crbug.com/697497
TEST(VectorIconTest, RelativeMoveToAfterClose) {
  cc::PaintRecorder recorder;
  Canvas canvas(recorder.beginRecording(100, 100), 1.0f);

  const PathElement elements[] = {
      MOVE_TO, 4, 5,
      LINE_TO, 10, 11,
      CLOSE,
      // This move should use (4, 5) as the start point rather than (10, 11).
      R_MOVE_TO, 20, 21,
      R_LINE_TO, 50, 51,
      END,
  };
  const VectorIcon icon = {elements, nullptr};

  PaintVectorIcon(&canvas, icon, 100, SK_ColorMAGENTA);
  sk_sp<cc::PaintRecord> record = recorder.finishRecordingAsPicture();

  MockCanvas mock(100, 100);
  record->Playback(&mock);

  ASSERT_EQ(1U, mock.paths().size());
  SkPoint last_point;
  EXPECT_TRUE(mock.paths()[0].getLastPt(&last_point));
  EXPECT_EQ(SkIntToScalar(74), last_point.x());
  EXPECT_EQ(SkIntToScalar(77), last_point.y());
}

TEST(VectorIconTest, FlipsInRtl) {
  // Set the locale to a rtl language otherwise FLIPS_IN_RTL will do nothing.
  base::i18n::SetICUDefaultLocale("he");
  ASSERT_TRUE(base::i18n::IsRTL());

  const int canvas_size = 20;
  const SkColor color = SK_ColorWHITE;

  Canvas canvas(gfx::Size(canvas_size, canvas_size), 1.0f, true);

  // Create a 20x20 square icon which has FLIPS_IN_RTL, and CANVAS_DIMENSIONS
  // are twice as large as |canvas|.
  const PathElement elements[] = {
      CANVAS_DIMENSIONS, 2 * canvas_size,
      FLIPS_IN_RTL,
      MOVE_TO, 10, 10,
      R_H_LINE_TO, 20,
      R_V_LINE_TO, 20,
      R_H_LINE_TO, -20,
      CLOSE,
      END,
  };
  const VectorIcon icon = {elements, nullptr};
  PaintVectorIcon(&canvas, icon, canvas_size, color);

  // Count the number of pixels in the canvas.
  auto bitmap = canvas.GetBitmap();
  int colored_pixel_count = 0;
  for (int i = 0; i < bitmap.width(); ++i) {
    for (int j = 0; j < bitmap.height(); ++j) {
      if (bitmap.getColor(i, j) == color)
        colored_pixel_count++;
    }
  }

  // Verify that the amount of colored pixels on the canvas bitmap should be a
  // quarter of the original icon, since each side should be scaled down by a
  // factor of two.
  EXPECT_EQ(100, colored_pixel_count);
}

}  // namespace

}  // namespace gfx
