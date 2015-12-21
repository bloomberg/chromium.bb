/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/graphics/Image.h"

#include "platform/graphics/GraphicsLayer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "wtf/PassOwnPtr.h"

namespace blink {

namespace {

class MockGraphicsLayerClient : public GraphicsLayerClient {
public:
    IntRect computeInterestRect(const GraphicsLayer*, const IntRect&) const { return IntRect(); }
    void paintContents(const GraphicsLayer*, GraphicsContext&, GraphicsLayerPaintingPhase, const IntRect&) const override { }
    String debugName(const GraphicsLayer*) const override { return String(); }
};

class TestImage : public Image {
public:
    static PassRefPtr<TestImage> create(const IntSize& size, bool opaque)
    {
        return adoptRef(new TestImage(size, opaque));
    }

    bool currentFrameKnownToBeOpaque(MetadataMode = UseCurrentMetadata) override
    {
        return m_image->isOpaque();
    }

    IntSize size() const override
    {
        return m_size;
    }

    PassRefPtr<SkImage> imageForCurrentFrame() override
    {
        return m_image;
    }

    void destroyDecodedData(bool) override
    {
        // Image pure virtual stub.
    }

    void draw(SkCanvas*, const SkPaint&, const FloatRect&, const FloatRect&, RespectImageOrientationEnum, ImageClampingMode) override
    {
        // Image pure virtual stub.
    }

private:
    TestImage(IntSize size, bool opaque)
        : Image(0)
        , m_size(size)
    {
        RefPtr<SkSurface> surface = adoptRef(createSkSurface(size, opaque));
        if (!surface)
            return;

        surface->getCanvas()->clear(SK_ColorTRANSPARENT);
        m_image = adoptRef(surface->newImageSnapshot());
    }

    static SkSurface* createSkSurface(IntSize size, bool opaque)
    {
        return SkSurface::NewRaster(SkImageInfo::MakeN32(size.width(), size.height(), opaque ? kOpaque_SkAlphaType : kPremul_SkAlphaType));
    }

    IntSize m_size;
    RefPtr<SkImage> m_image;
};

class GraphicsLayerForTesting : public GraphicsLayer {
public:
    explicit GraphicsLayerForTesting(GraphicsLayerClient* client)
        : GraphicsLayer(client)
    {
    }
};

} // anonymous namespace

TEST(ImageLayerChromiumTest, imageLayerContentReset)
{
    MockGraphicsLayerClient client;
    OwnPtr<GraphicsLayerForTesting> graphicsLayer = adoptPtr(new GraphicsLayerForTesting(&client));
    ASSERT_TRUE(graphicsLayer.get());

    ASSERT_FALSE(graphicsLayer->hasContentsLayer());
    ASSERT_FALSE(graphicsLayer->contentsLayer());

    bool opaque = false;
    RefPtr<Image> image = TestImage::create(IntSize(100, 100), opaque);
    ASSERT_TRUE(image.get());

    graphicsLayer->setContentsToImage(image.get());
    ASSERT_TRUE(graphicsLayer->hasContentsLayer());
    ASSERT_TRUE(graphicsLayer->contentsLayer());

    graphicsLayer->setContentsToImage(0);
    ASSERT_FALSE(graphicsLayer->hasContentsLayer());
    ASSERT_FALSE(graphicsLayer->contentsLayer());
}

TEST(ImageLayerChromiumTest, opaqueImages)
{
    MockGraphicsLayerClient client;
    OwnPtr<GraphicsLayerForTesting> graphicsLayer = adoptPtr(new GraphicsLayerForTesting(&client));
    ASSERT_TRUE(graphicsLayer.get());

    bool opaque = true;
    RefPtr<Image> opaqueImage = TestImage::create(IntSize(100, 100), opaque);
    ASSERT_TRUE(opaqueImage.get());
    RefPtr<Image> nonOpaqueImage = TestImage::create(IntSize(100, 100), !opaque);
    ASSERT_TRUE(nonOpaqueImage.get());

    ASSERT_FALSE(graphicsLayer->contentsLayer());

    graphicsLayer->setContentsToImage(opaqueImage.get());
    ASSERT_TRUE(graphicsLayer->contentsLayer()->opaque());

    graphicsLayer->setContentsToImage(nonOpaqueImage.get());
    ASSERT_FALSE(graphicsLayer->contentsLayer()->opaque());
}

} // namespace blink
