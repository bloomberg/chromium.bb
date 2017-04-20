// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/testing/PictureMatchers.h"

#include <utility>

#include "platform/geometry/FloatQuad.h"
#include "platform/geometry/FloatRect.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPicture.h"

namespace blink {

namespace {

struct QuadWithColor {
  FloatQuad quad;
  Color color;
};

class DrawsRectangleCanvas : public SkCanvas {
 public:
  DrawsRectangleCanvas()
      : SkCanvas(800, 600),
        save_count_(0),
        alpha_(255),
        alpha_save_layer_count_(-1) {}
  const Vector<QuadWithColor>& QuadsWithColor() const { return quads_; }

  void onDrawRect(const SkRect& rect, const SkPaint& paint) override {
    SkRect clipped_rect(rect);
    for (Vector<ClipAndIndex>::const_reverse_iterator clip = clips_.rbegin();
         clip != clips_.rend(); clip++) {
      if (SkRect::Intersects(rect, clip->rect))
        CHECK(clipped_rect.intersect(clip->rect));
    }
    SkPoint quad[4];
    getTotalMatrix().mapRectToQuad(quad, clipped_rect);
    QuadWithColor quad_with_color;
    quad_with_color.quad = FloatQuad(quad);

    unsigned paint_alpha = static_cast<unsigned>(paint.getAlpha());
    SkPaint paint_with_alpha(paint);
    paint_with_alpha.setAlpha(static_cast<U8CPU>(alpha_ * paint_alpha / 255));
    quad_with_color.color = Color(paint_with_alpha.getColor());
    quads_.push_back(quad_with_color);
    SkCanvas::onDrawRect(clipped_rect, paint);
  }

  SkCanvas::SaveLayerStrategy getSaveLayerStrategy(
      const SaveLayerRec& rec) override {
    save_count_++;
    unsigned layer_alpha = static_cast<unsigned>(rec.fPaint->getAlpha());
    if (layer_alpha < 255) {
      DCHECK_EQ(alpha_save_layer_count_, -1);
      alpha_save_layer_count_ = save_count_;
      alpha_ = layer_alpha;
    }
    return SkCanvas::getSaveLayerStrategy(rec);
  }

  void willSave() override {
    save_count_++;
    SkCanvas::willSave();
  }

  void willRestore() override {
    DCHECK_GT(save_count_, 0);
    if (clips_.size() && save_count_ == clips_.back().save_count)
      clips_.pop_back();
    if (alpha_save_layer_count_ == save_count_) {
      alpha_ = 255;
      alpha_save_layer_count_ = -1;
    }
    save_count_--;
    SkCanvas::willRestore();
  }

  void onClipRect(const SkRect& rect,
                  SkClipOp op,
                  ClipEdgeStyle style) override {
    ClipAndIndex clip_struct;
    clip_struct.rect = rect;
    clip_struct.save_count = save_count_;
    clips_.push_back(clip_struct);
    SkCanvas::onClipRect(rect, op, style);
  }

  struct ClipAndIndex {
    SkRect rect;
    int save_count;
  };

 private:
  Vector<QuadWithColor> quads_;
  Vector<ClipAndIndex> clips_;
  int save_count_;
  unsigned alpha_;
  int alpha_save_layer_count_;
};

class DrawsRectanglesMatcher
    : public ::testing::MatcherInterface<const SkPicture&> {
 public:
  DrawsRectanglesMatcher(const Vector<RectWithColor>& rects_with_color)
      : rects_with_color_(rects_with_color) {}

  bool MatchAndExplain(
      const SkPicture& picture,
      ::testing::MatchResultListener* listener) const override {
    DrawsRectangleCanvas canvas;
    picture.playback(&canvas);
    const auto& quads = canvas.QuadsWithColor();
    if (quads.size() != rects_with_color_.size()) {
      *listener << "which draws " << quads.size() << " quads";
      return false;
    }

    for (unsigned index = 0; index < quads.size(); index++) {
      const auto& quad_with_color = quads[index];
      const auto& rect_with_color = rects_with_color_[index];

      const FloatRect& rect = quad_with_color.quad.BoundingBox();
      if (EnclosingIntRect(rect) != EnclosingIntRect(rect_with_color.rect) ||
          quad_with_color.color != rect_with_color.color) {
        if (listener->IsInterested()) {
          *listener << "at index " << index << " which draws ";
          PrintTo(rect, listener->stream());
          *listener << " with color "
                    << quad_with_color.color.Serialized().Ascii().data()
                    << "\n";
        }
        return false;
      }
    }

    return true;
  }

  void DescribeTo(::std::ostream* os) const override {
    *os << "\n";
    for (unsigned index = 0; index < rects_with_color_.size(); index++) {
      const auto& rect_with_color = rects_with_color_[index];
      *os << "at index " << index << " rect draws ";
      PrintTo(rect_with_color.rect, os);
      *os << " with color " << rect_with_color.color.Serialized().Ascii().data()
          << "\n";
    }
  }

 private:
  const Vector<RectWithColor> rects_with_color_;
};

}  // namespace

::testing::Matcher<const SkPicture&> DrawsRectangle(const FloatRect& rect,
                                                    Color color) {
  Vector<RectWithColor> rects_with_color;
  rects_with_color.push_back(RectWithColor(rect, color));
  return ::testing::MakeMatcher(new DrawsRectanglesMatcher(rects_with_color));
}

::testing::Matcher<const SkPicture&> DrawsRectangles(
    const Vector<RectWithColor>& rects_with_color) {
  return ::testing::MakeMatcher(new DrawsRectanglesMatcher(rects_with_color));
}

}  // namespace blink
