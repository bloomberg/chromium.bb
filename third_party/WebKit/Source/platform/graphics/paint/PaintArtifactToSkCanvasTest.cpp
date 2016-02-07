// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/PaintArtifactToSkCanvas.h"

#include "platform/graphics/paint/DisplayItem.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"
#include "platform/graphics/paint/PaintArtifact.h"
#include "platform/transforms/TransformationMatrix.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/core/SkRect.h"

using testing::_;
using testing::Eq;
using testing::Pointee;
using testing::Property;
using testing::ResultOf;

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
    MOCK_METHOD2(willSaveLayer, void(unsigned alpha, MockCanvas*));

private:
    void onDrawRect(const SkRect& rect, const SkPaint& paint) override
    {
        onDrawRect(rect, paint, this);
    }

    SaveLayerStrategy getSaveLayerStrategy(const SaveLayerRec& rec) override
    {
        willSaveLayer(rec.fPaint->getAlpha(), this);
        return SaveLayerStrategy::kFullLayer_SaveLayerStrategy;
    }
};

class DummyRectClient : public DisplayItemClient {
public:
    DummyRectClient(const SkRect& rect, SkColor color) : m_rect(rect), m_color(color) {}
    const SkRect& rect() const { return m_rect; }
    SkColor color() const { return m_color; }
    PassRefPtr<SkPicture> makePicture() const { return pictureWithRect(m_rect, m_color); }
    String debugName() const final { return "<dummy>"; }
    LayoutRect visualRect() const override { return LayoutRect(); }

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

TEST(PaintArtifactToSkCanvasTest, OpacityEffectsCombining)
{
    DummyRectClient client1(SkRect::MakeXYWH(0, 0, 300, 200), SK_ColorRED);
    DummyRectClient client2(SkRect::MakeXYWH(0, 0, 300, 200), SK_ColorBLUE);

    unsigned expectedFirstOpacity = 127; // floor(0.5 * 255)
    unsigned expectedSecondOpacity = 63; // floor(0.25 * 255)
    MockCanvas canvas(kCanvasWidth, kCanvasHeight);
    {
        testing::InSequence sequence;
        EXPECT_CALL(canvas, willSaveLayer(expectedFirstOpacity, _));
        EXPECT_CALL(canvas, onDrawRect(client1.rect(), Property(&SkPaint::getColor, client1.color()), _));
        EXPECT_CALL(canvas, willSaveLayer(expectedSecondOpacity, _));
        EXPECT_CALL(canvas, onDrawRect(client2.rect(), Property(&SkPaint::getColor, client2.color()), _));
    }

    // Build an opacity effect tree.
    RefPtr<EffectPaintPropertyNode> opacityEffect1 = EffectPaintPropertyNode::create(0.5);
    RefPtr<EffectPaintPropertyNode> opacityEffect2 = EffectPaintPropertyNode::create(0.25, opacityEffect1);

    // Build a two-chunk artifact directly.
    PaintArtifact artifact;
    artifact.displayItemList().allocateAndConstruct<DrawingDisplayItem>(
        client1, DisplayItem::DrawingFirst, client1.makePicture());
    PaintChunk chunk1(0, 1, PaintChunkProperties());
    chunk1.properties.effect = opacityEffect1.get();
    artifact.paintChunks().append(chunk1);

    artifact.displayItemList().allocateAndConstruct<DrawingDisplayItem>(
        client2, DisplayItem::DrawingFirst, client2.makePicture());
    PaintChunk chunk2(1, 2, PaintChunkProperties());
    chunk2.properties.effect = opacityEffect2.get();
    artifact.paintChunks().append(chunk2);

    paintArtifactToSkCanvas(artifact, &canvas);
}

TEST(PaintArtifactToSkCanvasTest, ChangingOpacityEffects)
{
    DummyRectClient client1(SkRect::MakeXYWH(0, 0, 300, 200), SK_ColorRED);
    DummyRectClient client2(SkRect::MakeXYWH(0, 0, 300, 200), SK_ColorBLUE);

    unsigned expectedAOpacity = 25; // floor(0.1 * 255)
    unsigned expectedBOpacity = 51; // floor(0.2 * 255)
    unsigned expectedCOpacity = 76; // floor(0.3 * 255)
    unsigned expectedDOpacity = 102; // floor(0.4 * 255)
    MockCanvas canvas(kCanvasWidth, kCanvasHeight);
    {
        testing::InSequence sequence;
        EXPECT_CALL(canvas, willSaveLayer(expectedAOpacity, _));
        EXPECT_CALL(canvas, willSaveLayer(expectedBOpacity, _));
        EXPECT_CALL(canvas, onDrawRect(client1.rect(), Property(&SkPaint::getColor, client1.color()), _));
        EXPECT_CALL(canvas, willSaveLayer(expectedCOpacity, _));
        EXPECT_CALL(canvas, willSaveLayer(expectedDOpacity, _));
        EXPECT_CALL(canvas, onDrawRect(client2.rect(), Property(&SkPaint::getColor, client2.color()), _));
    }

    // Build an opacity effect tree with the following structure:
    //       _root_
    //      |      |
    // 0.1  a      c  0.3
    //      |      |
    // 0.2  b      d  0.4
    RefPtr<EffectPaintPropertyNode> opacityEffectA = EffectPaintPropertyNode::create(0.1);
    RefPtr<EffectPaintPropertyNode> opacityEffectB = EffectPaintPropertyNode::create(0.2, opacityEffectA);
    RefPtr<EffectPaintPropertyNode> opacityEffectC = EffectPaintPropertyNode::create(0.3);
    RefPtr<EffectPaintPropertyNode> opacityEffectD = EffectPaintPropertyNode::create(0.4, opacityEffectC);

    // Build a two-chunk artifact directly.
    // chunk1 references opacity node b, chunk2 references opacity node d.
    PaintArtifact artifact;
    artifact.displayItemList().allocateAndConstruct<DrawingDisplayItem>(
        client1, DisplayItem::DrawingFirst, client1.makePicture());
    PaintChunk chunk1(0, 1, PaintChunkProperties());
    chunk1.properties.effect = opacityEffectB.get();
    artifact.paintChunks().append(chunk1);

    artifact.displayItemList().allocateAndConstruct<DrawingDisplayItem>(
        client2, DisplayItem::DrawingFirst, client2.makePicture());
    PaintChunk chunk2(1, 2, PaintChunkProperties());
    chunk2.properties.effect = opacityEffectD.get();
    artifact.paintChunks().append(chunk2);

    paintArtifactToSkCanvas(artifact, &canvas);
}

static SkRegion getCanvasClipAsRegion(SkCanvas* canvas)
{
    return SkCanvas::LayerIter(canvas, false).clip();
}

TEST(PaintArtifactToSkCanvasTest, ClipWithScrollEscaping)
{
    // The setup is to simulate scenario similar to this html:
    // <div style="position:absolute; left:0; top:0; clip:rect(200px,200px,300px,100px);">
    //     <div style="position:fixed; left:150px; top:150px; width:100px; height:100px; overflow:hidden;">
    //         client1
    //     </div>
    // </div>
    // <script>scrollTo(0, 100)</script>
    // The content itself will not be scrolled due to fixed positioning,
    // but will be affected by some scrolled clip.

    // Setup transform tree.
    RefPtr<TransformPaintPropertyNode> transform1 = TransformPaintPropertyNode::create(
        TransformationMatrix().translate(0, -100), FloatPoint3D());

    // Setup clip tree.
    RefPtr<ClipPaintPropertyNode> clip1 = ClipPaintPropertyNode::create(
        transform1.get(), FloatRoundedRect(100, 200, 100, 100));
    RefPtr<ClipPaintPropertyNode> clip2 = ClipPaintPropertyNode::create(
        nullptr, FloatRoundedRect(150, 150, 100, 100), clip1.get());

    PaintArtifact artifact;

    DummyRectClient client1(SkRect::MakeXYWH(0, 0, 300, 200), SK_ColorRED);
    artifact.displayItemList().allocateAndConstruct<DrawingDisplayItem>(
        client1, DisplayItem::DrawingFirst, client1.makePicture());
    PaintChunk chunk1(0, 1, PaintChunkProperties());
    chunk1.properties.clip = clip2.get();
    artifact.paintChunks().append(chunk1);

    MockCanvas canvas(kCanvasWidth, kCanvasHeight);

    SkRegion totalClip;
    totalClip.setRect(SkIRect::MakeXYWH(100, 100, 100, 100));
    totalClip.op(SkIRect::MakeXYWH(150, 150, 100, 100), SkRegion::kIntersect_Op);
    EXPECT_CALL(canvas, onDrawRect(client1.rect(), Property(&SkPaint::getColor, client1.color()),
        ResultOf(&getCanvasClipAsRegion, Eq(totalClip))));

    paintArtifactToSkCanvas(artifact, &canvas);
}

} // namespace
} // namespace blink
