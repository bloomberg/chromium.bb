// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/compositing/PaintArtifactCompositor.h"

#include "cc/layers/layer.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/paint/PaintArtifact.h"
#include "platform/testing/PictureMatchers.h"
#include "platform/testing/TestPaintArtifact.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

using ::testing::Pointee;

gfx::Transform translation(SkMScalar x, SkMScalar y)
{
    gfx::Transform transform;
    transform.Translate(x, y);
    return transform;
}

class PaintArtifactCompositorTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        RuntimeEnabledFeatures::setSlimmingPaintV2Enabled(true);
        m_paintArtifactCompositor.initializeIfNeeded();
    }

    void TearDown() override
    {
        m_featuresBackup.restore();
    }

    PaintArtifactCompositor& paintArtifactCompositor() { return m_paintArtifactCompositor; }
    cc::Layer* rootLayer() { return m_paintArtifactCompositor.rootLayer(); }
    void update(const PaintArtifact& artifact) { m_paintArtifactCompositor.update(artifact); }

private:
    RuntimeEnabledFeatures::Backup m_featuresBackup;
    PaintArtifactCompositor m_paintArtifactCompositor;
};

TEST_F(PaintArtifactCompositorTest, EmptyPaintArtifact)
{
    PaintArtifact emptyArtifact;
    update(emptyArtifact);
    EXPECT_TRUE(rootLayer()->children().empty());
}

TEST_F(PaintArtifactCompositorTest, OneChunkWithAnOffset)
{
    TestPaintArtifact artifact;
    artifact.chunk(PaintChunkProperties())
        .rectDrawing(FloatRect(50, -50, 100, 100), Color::white);
    update(artifact.build());

    ASSERT_EQ(1u, rootLayer()->children().size());
    const cc::Layer* child = rootLayer()->child_at(0);
    EXPECT_THAT(child->GetPicture(),
        Pointee(drawsRectangle(FloatRect(0, 0, 100, 100), Color::white)));
    EXPECT_EQ(translation(50, -50), child->transform());
}

TEST_F(PaintArtifactCompositorTest, OneTransform)
{
    // A 90 degree clockwise rotation about (100, 100).
    RefPtr<TransformPaintPropertyNode> transform = TransformPaintPropertyNode::create(
        TransformationMatrix().rotate(90), FloatPoint3D(100, 100, 0));

    TestPaintArtifact artifact;
    artifact.chunk(transform, nullptr, nullptr)
        .rectDrawing(FloatRect(0, 0, 100, 100), Color::white);
    artifact.chunk(nullptr, nullptr, nullptr)
        .rectDrawing(FloatRect(0, 0, 100, 100), Color::gray);
    artifact.chunk(transform, nullptr, nullptr)
        .rectDrawing(FloatRect(100, 100, 200, 100), Color::black);
    update(artifact.build());

    ASSERT_EQ(3u, rootLayer()->children().size());
    {
        const cc::Layer* layer = rootLayer()->child_at(0);
        EXPECT_THAT(layer->GetPicture(),
            Pointee(drawsRectangle(FloatRect(0, 0, 100, 100), Color::white)));
        gfx::RectF mappedRect(0, 0, 100, 100);
        layer->transform().TransformRect(&mappedRect);
        EXPECT_EQ(gfx::RectF(100, 0, 100, 100), mappedRect);
    }
    {
        const cc::Layer* layer = rootLayer()->child_at(1);
        EXPECT_THAT(layer->GetPicture(),
            Pointee(drawsRectangle(FloatRect(0, 0, 100, 100), Color::gray)));
        EXPECT_EQ(gfx::Transform(), layer->transform());
    }
    {
        const cc::Layer* layer = rootLayer()->child_at(2);
        EXPECT_THAT(layer->GetPicture(),
            Pointee(drawsRectangle(FloatRect(0, 0, 200, 100), Color::black)));
        gfx::RectF mappedRect(0, 0, 200, 100);
        layer->transform().TransformRect(&mappedRect);
        EXPECT_EQ(gfx::RectF(0, 100, 100, 200), mappedRect);
    }
}

TEST_F(PaintArtifactCompositorTest, TransformCombining)
{
    // A translation by (5, 5) within a 2x scale about (10, 10).
    RefPtr<TransformPaintPropertyNode> transform1 = TransformPaintPropertyNode::create(
        TransformationMatrix().scale(2), FloatPoint3D(10, 10, 0));
    RefPtr<TransformPaintPropertyNode> transform2 = TransformPaintPropertyNode::create(
        TransformationMatrix().translate(5, 5), FloatPoint3D(), transform1);

    TestPaintArtifact artifact;
    artifact.chunk(transform1, nullptr, nullptr)
        .rectDrawing(FloatRect(0, 0, 300, 200), Color::white);
    artifact.chunk(transform2, nullptr, nullptr)
        .rectDrawing(FloatRect(0, 0, 300, 200), Color::black);
    update(artifact.build());

    ASSERT_EQ(2u, rootLayer()->children().size());
    {
        const cc::Layer* layer = rootLayer()->child_at(0);
        EXPECT_THAT(layer->GetPicture(),
            Pointee(drawsRectangle(FloatRect(0, 0, 300, 200), Color::white)));
        gfx::RectF mappedRect(0, 0, 300, 200);
        layer->transform().TransformRect(&mappedRect);
        EXPECT_EQ(gfx::RectF(-10, -10, 600, 400), mappedRect);
    }
    {
        const cc::Layer* layer = rootLayer()->child_at(1);
        EXPECT_THAT(layer->GetPicture(),
            Pointee(drawsRectangle(FloatRect(0, 0, 300, 200), Color::black)));
        gfx::RectF mappedRect(0, 0, 300, 200);
        layer->transform().TransformRect(&mappedRect);
        EXPECT_EQ(gfx::RectF(0, 0, 600, 400), mappedRect);
    }
}

} // namespace
} // namespace blink
