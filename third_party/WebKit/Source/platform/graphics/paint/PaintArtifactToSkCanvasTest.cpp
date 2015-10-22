// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/graphics/paint/PaintArtifactToSkCanvas.h"

#include "platform/graphics/paint/DisplayItem.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"
#include "platform/graphics/paint/PaintArtifact.h"
#include "platform/transforms/TransformationMatrix.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/core/SkRect.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using testing::_;
using testing::Pointee;
using testing::Property;

namespace blink {
namespace {

static const int kCanvasWidth = 800;
static const int kCanvasHeight = 600;

static PassRefPtr<SkPicture> pictureWithRect(const SkRect& rect, SkColor color)
{
    SkPictureRecorder recorder;
    SkCanvas* canvas = recorder.beginRecording(rect);
    SkPaint paint;
    paint.setColor(color);
    canvas->drawRect(rect, paint);
    return adoptRef(recorder.endRecordingAsPicture());
}

class MockCanvas : public SkCanvas {
public:
    MockCanvas(int width, int height) : SkCanvas(width, height) {}

    MOCK_METHOD3(onDrawRect, void(const SkRect&, const SkPaint&, MockCanvas*));

private:
    void onDrawRect(const SkRect& rect, const SkPaint& paint) override
    {
        onDrawRect(rect, paint, this);
    }
};

class DummyRectClient {
public:
    DummyRectClient(const SkRect& rect, SkColor color) : m_rect(rect), m_color(color) {}
    const SkRect& rect() const { return m_rect; }
    SkColor color() const { return m_color; }
    PassRefPtr<SkPicture> makePicture() const { return pictureWithRect(m_rect, m_color); }
    DisplayItemClient displayItemClient() const { return toDisplayItemClient(this); }
    String debugName() const { return "<dummy>"; }

private:
    SkRect m_rect;
    SkColor m_color;
};

TEST(PaintArtifactToSkCanvasTest, Empty)
{
    MockCanvas canvas(kCanvasWidth, kCanvasHeight);
    EXPECT_CALL(canvas, onDrawRect(_, _, _)).Times(0);

    PaintArtifact artifact;
    paintArtifactToSkCanvas(artifact, &canvas);
}

TEST(PaintArtifactToSkCanvasTest, OneChunkWithDrawingsInOrder)
{
    DummyRectClient client1(SkRect::MakeXYWH(100, 100, 100, 100), SK_ColorRED);
    DummyRectClient client2(SkRect::MakeXYWH(100, 150, 300, 200), SK_ColorBLUE);

    MockCanvas canvas(kCanvasWidth, kCanvasHeight);
    {
        testing::InSequence sequence;
        EXPECT_CALL(canvas, onDrawRect(client1.rect(), Property(&SkPaint::getColor, client1.color()), _));
        EXPECT_CALL(canvas, onDrawRect(client2.rect(), Property(&SkPaint::getColor, client2.color()), _));
    }

    // Build a single-chunk artifact directly.
    PaintArtifact artifact;
    artifact.displayItemList().allocateAndConstruct<DrawingDisplayItem>(
        client1, DisplayItem::DrawingFirst, client1.makePicture());
    artifact.displayItemList().allocateAndConstruct<DrawingDisplayItem>(
        client2, DisplayItem::DrawingFirst, client2.makePicture());
    PaintChunk chunk(0, artifact.displayItemList().size(), PaintChunkProperties());
    artifact.paintChunks().append(chunk);

    paintArtifactToSkCanvas(artifact, &canvas);
}

TEST(PaintArtifactToSkCanvasTest, TransformCombining)
{
    DummyRectClient client1(SkRect::MakeXYWH(0, 0, 300, 200), SK_ColorRED);
    DummyRectClient client2(SkRect::MakeXYWH(0, 0, 300, 200), SK_ColorBLUE);

    // We expect a matrix which applies the inner translation to the points
    // first, followed by the origin-adjusted scale.
    SkMatrix adjustedScale;
    adjustedScale.setTranslate(-10, -10);
    adjustedScale.postScale(2, 2);
    adjustedScale.postTranslate(10, 10);
    SkMatrix combinedMatrix(adjustedScale);
    combinedMatrix.preTranslate(5, 5);
    MockCanvas canvas(kCanvasWidth, kCanvasHeight);
    {
        testing::InSequence sequence;
        EXPECT_CALL(canvas, onDrawRect(client1.rect(), Property(&SkPaint::getColor, client1.color()),
            Pointee(Property(&SkCanvas::getTotalMatrix, adjustedScale))));
        EXPECT_CALL(canvas, onDrawRect(client2.rect(), Property(&SkPaint::getColor, client2.color()),
            Pointee(Property(&SkCanvas::getTotalMatrix, combinedMatrix))));
    }

    // Build the transform tree.
    TransformationMatrix matrix1;
    matrix1.scale(2);
    FloatPoint3D origin1(10, 10, 0);
    RefPtr<TransformPaintPropertyNode> transform1 = TransformPaintPropertyNode::create(matrix1, origin1);
    TransformationMatrix matrix2;
    matrix2.translate(5, 5);
    RefPtr<TransformPaintPropertyNode> transform2 = TransformPaintPropertyNode::create(matrix2, FloatPoint3D(), transform1.get());

    // Build a two-chunk artifact directly.
    PaintArtifact artifact;

    artifact.displayItemList().allocateAndConstruct<DrawingDisplayItem>(
        client1, DisplayItem::DrawingFirst, client1.makePicture());
    PaintChunk chunk1(0, 1, PaintChunkProperties());
    chunk1.properties.transform = transform1.get();
    artifact.paintChunks().append(chunk1);

    artifact.displayItemList().allocateAndConstruct<DrawingDisplayItem>(
        client2, DisplayItem::DrawingFirst, client2.makePicture());
    PaintChunk chunk2(1, 2, PaintChunkProperties());
    chunk2.properties.transform = transform2.get();
    artifact.paintChunks().append(chunk2);

    paintArtifactToSkCanvas(artifact, &canvas);
}

} // namespace
} // namespace blink
