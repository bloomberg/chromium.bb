// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/testing/PictureMatchers.h"

#include <utility>

#include "platform/geometry/FloatQuad.h"
#include "platform/geometry/FloatRect.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

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
        m_saveCount(0),
        m_alpha(255),
        m_alphaSaveLayerCount(-1) {}
  const Vector<QuadWithColor>& quadsWithColor() const { return m_quads; }

  void onDrawRect(const SkRect& rect, const SkPaint& paint) override {
    SkRect clippedRect(rect);
    for (Vector<ClipAndIndex>::const_reverse_iterator clip = m_clips.rbegin();
         clip != m_clips.rend(); clip++) {
      if (SkRect::Intersects(rect, clip->rect))
        CHECK(clippedRect.intersect(clip->rect));
    }
    SkPoint quad[4];
    getTotalMatrix().mapRectToQuad(quad, clippedRect);
    QuadWithColor quadWithColor;
    quadWithColor.quad = FloatQuad(quad);

    unsigned paintAlpha = static_cast<unsigned>(paint.getAlpha());
    SkPaint paintWithAlpha(paint);
    paintWithAlpha.setAlpha(static_cast<U8CPU>(m_alpha * paintAlpha / 255));
    quadWithColor.color = Color(paintWithAlpha.getColor());
    m_quads.push_back(quadWithColor);
    SkCanvas::onDrawRect(clippedRect, paint);
  }

  SkCanvas::SaveLayerStrategy getSaveLayerStrategy(
      const SaveLayerRec& rec) override {
    m_saveCount++;
    unsigned layerAlpha = static_cast<unsigned>(rec.fPaint->getAlpha());
    if (layerAlpha < 255) {
      DCHECK_EQ(m_alphaSaveLayerCount, -1);
      m_alphaSaveLayerCount = m_saveCount;
      m_alpha = layerAlpha;
    }
    return SkCanvas::getSaveLayerStrategy(rec);
  }

  void willSave() override {
    m_saveCount++;
    SkCanvas::willSave();
  }

  void willRestore() override {
    DCHECK_GT(m_saveCount, 0);
    if (m_clips.size() && m_saveCount == m_clips.back().saveCount)
      m_clips.pop_back();
    if (m_alphaSaveLayerCount == m_saveCount) {
      m_alpha = 255;
      m_alphaSaveLayerCount = -1;
    }
    m_saveCount--;
    SkCanvas::willRestore();
  }

  void onClipRect(const SkRect& rect,
                  SkClipOp op,
                  ClipEdgeStyle style) override {
    ClipAndIndex clipStruct;
    clipStruct.rect = rect;
    clipStruct.saveCount = m_saveCount;
    m_clips.push_back(clipStruct);
    SkCanvas::onClipRect(rect, op, style);
  }

  struct ClipAndIndex {
    SkRect rect;
    int saveCount;
  };

 private:
  Vector<QuadWithColor> m_quads;
  Vector<ClipAndIndex> m_clips;
  int m_saveCount;
  unsigned m_alpha;
  int m_alphaSaveLayerCount;
};

class DrawsRectanglesMatcher
    : public ::testing::MatcherInterface<const SkPicture&> {
 public:
  DrawsRectanglesMatcher(const Vector<RectWithColor>& rectsWithColor)
      : m_rectsWithColor(rectsWithColor) {}

  bool MatchAndExplain(
      const SkPicture& picture,
      ::testing::MatchResultListener* listener) const override {
    DrawsRectangleCanvas canvas;
    picture.playback(&canvas);
    const auto& quads = canvas.quadsWithColor();
    if (quads.size() != m_rectsWithColor.size()) {
      *listener << "which draws " << quads.size() << " quads";
      return false;
    }

    for (unsigned index = 0; index < quads.size(); index++) {
      const auto& quadWithColor = quads[index];
      const auto& rectWithColor = m_rectsWithColor[index];

      const FloatRect& rect = quadWithColor.quad.boundingBox();
      if (enclosingIntRect(rect) != enclosingIntRect(rectWithColor.rect) ||
          quadWithColor.color != rectWithColor.color) {
        if (listener->IsInterested()) {
          *listener << "at index " << index << " which draws ";
          PrintTo(rect, listener->stream());
          *listener << " with color "
                    << quadWithColor.color.serialized().ascii().data() << "\n";
        }
        return false;
      }
    }

    return true;
  }

  void DescribeTo(::std::ostream* os) const override {
    *os << "\n";
    for (unsigned index = 0; index < m_rectsWithColor.size(); index++) {
      const auto& rectWithColor = m_rectsWithColor[index];
      *os << "at index " << index << " rect draws ";
      PrintTo(rectWithColor.rect, os);
      *os << " with color " << rectWithColor.color.serialized().ascii().data()
          << "\n";
    }
  }

 private:
  const Vector<RectWithColor> m_rectsWithColor;
};

}  // namespace

::testing::Matcher<const SkPicture&> drawsRectangle(const FloatRect& rect,
                                                    Color color) {
  Vector<RectWithColor> rectsWithColor;
  rectsWithColor.push_back(RectWithColor(rect, color));
  return ::testing::MakeMatcher(new DrawsRectanglesMatcher(rectsWithColor));
}

::testing::Matcher<const SkPicture&> drawsRectangles(
    const Vector<RectWithColor>& rectsWithColor) {
  return ::testing::MakeMatcher(new DrawsRectanglesMatcher(rectsWithColor));
}

}  // namespace blink
