// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "core/html/canvas/CanvasRenderingContext2D.h"

#include "core/frame/FrameView.h"
#include "core/html/HTMLDocument.h"
#include "core/html/ImageData.h"
#include "core/html/canvas/CanvasGradient.h"
#include "core/html/canvas/CanvasPattern.h"
#include "core/html/canvas/WebGLRenderingContext.h"
#include "core/loader/EmptyClients.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/graphics/ExpensiveCanvasHeuristicParameters.h"
#include "platform/graphics/RecordingImageBufferSurface.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "platform/graphics/UnacceleratedImageBufferSurface.h"
#include "third_party/skia/include/core/SkSurface.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace blink;
using ::testing::Mock;

namespace {

enum BitmapOpacity {
    OpaqueBitmap,
    TransparentBitmap
};

class FakeImageSource : public CanvasImageSource {
public:
    FakeImageSource(IntSize, BitmapOpacity);

    PassRefPtr<Image> getSourceImageForCanvas(SourceImageMode, SourceImageStatus*) const override;

    bool wouldTaintOrigin(SecurityOrigin* destinationSecurityOrigin) const override { return false; }
    FloatSize sourceSize() const override { return FloatSize(m_size); }
    bool isOpaque() const override { return m_isOpaque; }

    virtual ~FakeImageSource() { }

private:
    IntSize m_size;
    RefPtr<Image> m_image;
    bool m_isOpaque;
};

FakeImageSource::FakeImageSource(IntSize size, BitmapOpacity opacity)
    : m_size(size)
    , m_isOpaque(opacity == OpaqueBitmap)
{
    SkAutoTUnref<SkSurface> surface(SkSurface::NewRasterN32Premul(m_size.width(), m_size.height()));
    surface->getCanvas()->clear(opacity == OpaqueBitmap ? SK_ColorWHITE : SK_ColorTRANSPARENT);
    RefPtr<SkImage> image = adoptRef(surface->newImageSnapshot());
    m_image = StaticBitmapImage::create(image);
}

PassRefPtr<Image> FakeImageSource::getSourceImageForCanvas(SourceImageMode, SourceImageStatus* status) const
{
    if (status)
        *status = NormalSourceImageStatus;
    return m_image;
}

//============================================================================

class CanvasRenderingContext2DTest : public ::testing::Test {
protected:
    CanvasRenderingContext2DTest();
    virtual void SetUp() override;

    DummyPageHolder& page() const { return *m_dummyPageHolder; }
    HTMLDocument& document() const { return *m_document; }
    HTMLCanvasElement& canvasElement() const { return *m_canvasElement; }
    CanvasRenderingContext2D* context2d() const { return static_cast<CanvasRenderingContext2D*>(canvasElement().renderingContext()); }

    void createContext(OpacityMode);

private:
    OwnPtr<DummyPageHolder> m_dummyPageHolder;
    RefPtrWillBePersistent<HTMLDocument> m_document;
    RefPtrWillBePersistent<HTMLCanvasElement> m_canvasElement;

protected:
    // Pre-canned objects for testing
    RefPtrWillBePersistent<ImageData> m_fullImageData;
    RefPtrWillBePersistent<ImageData> m_partialImageData;
    FakeImageSource m_opaqueBitmap;
    FakeImageSource m_alphaBitmap;
    StringOrCanvasGradientOrCanvasPattern m_opaqueGradient;
    StringOrCanvasGradientOrCanvasPattern m_alphaGradient;
};

CanvasRenderingContext2DTest::CanvasRenderingContext2DTest()
    : m_opaqueBitmap(IntSize(10, 10), OpaqueBitmap)
    , m_alphaBitmap(IntSize(10, 10), TransparentBitmap)
{ }

void CanvasRenderingContext2DTest::createContext(OpacityMode opacityMode)
{
    String canvasType("2d");
    CanvasContextCreationAttributes attributes;
    attributes.setAlpha(opacityMode == NonOpaque);
    CanvasRenderingContext2DOrWebGLRenderingContext result;
    m_canvasElement->getContext(canvasType, attributes, result);
}

void CanvasRenderingContext2DTest::SetUp()
{
    Page::PageClients pageClients;
    fillWithEmptyClients(pageClients);
    m_dummyPageHolder = DummyPageHolder::create(IntSize(800, 600), &pageClients);
    m_document = toHTMLDocument(&m_dummyPageHolder->document());
    m_document->documentElement()->setInnerHTML("<body><canvas id='c'></canvas></body>", ASSERT_NO_EXCEPTION);
    m_document->view()->updateLayoutAndStyleIfNeededRecursive();
    m_canvasElement = toHTMLCanvasElement(m_document->getElementById("c"));

    m_fullImageData = ImageData::create(IntSize(10, 10));
    m_partialImageData = ImageData::create(IntSize(2, 2));

    NonThrowableExceptionState exceptionState;
    RefPtrWillBeRawPtr<CanvasGradient> opaqueGradient = CanvasGradient::create(FloatPoint(0, 0), FloatPoint(10, 0));
    opaqueGradient->addColorStop(0, String("green"), exceptionState);
    EXPECT_FALSE(exceptionState.hadException());
    opaqueGradient->addColorStop(1, String("blue"), exceptionState);
    EXPECT_FALSE(exceptionState.hadException());
    m_opaqueGradient.setCanvasGradient(opaqueGradient);

    RefPtrWillBeRawPtr<CanvasGradient> alphaGradient = CanvasGradient::create(FloatPoint(0, 0), FloatPoint(10, 0));
    alphaGradient->addColorStop(0, String("green"), exceptionState);
    EXPECT_FALSE(exceptionState.hadException());
    alphaGradient->addColorStop(1, String("rgba(0, 0, 255, 0.5)"), exceptionState);
    EXPECT_FALSE(exceptionState.hadException());
    StringOrCanvasGradientOrCanvasPattern wrappedAlphaGradient;
    m_alphaGradient.setCanvasGradient(alphaGradient);
}

//============================================================================

class MockImageBufferSurfaceForOverwriteTesting : public UnacceleratedImageBufferSurface {
public:
    MockImageBufferSurfaceForOverwriteTesting(const IntSize& size, OpacityMode mode) : UnacceleratedImageBufferSurface(size, mode) { }
    virtual ~MockImageBufferSurfaceForOverwriteTesting() { }
    bool isRecording() const override { return true; } // otherwise overwrites are not tracked

    MOCK_METHOD0(willOverwriteCanvas, void());
};

//============================================================================

#define TEST_OVERDRAW_SETUP(EXPECTED_OVERDRAWS) \
        OwnPtr<MockImageBufferSurfaceForOverwriteTesting> mockSurface = adoptPtr(new MockImageBufferSurfaceForOverwriteTesting(IntSize(10, 10), NonOpaque)); \
        MockImageBufferSurfaceForOverwriteTesting* surfacePtr = mockSurface.get(); \
        canvasElement().createImageBufferUsingSurface(mockSurface.release()); \
        EXPECT_CALL(*surfacePtr, willOverwriteCanvas()).Times(EXPECTED_OVERDRAWS); \
        context2d()->save();

#define TEST_OVERDRAW_FINALIZE \
        context2d()->restore(); \
        Mock::VerifyAndClearExpectations(surfacePtr);

#define TEST_OVERDRAW_1(EXPECTED_OVERDRAWS, CALL1) \
    do { \
        TEST_OVERDRAW_SETUP(EXPECTED_OVERDRAWS) \
        context2d()->CALL1; \
        TEST_OVERDRAW_FINALIZE \
    } while (0)

#define TEST_OVERDRAW_2(EXPECTED_OVERDRAWS, CALL1, CALL2) \
    do { \
        TEST_OVERDRAW_SETUP(EXPECTED_OVERDRAWS) \
        context2d()->CALL1; \
        context2d()->CALL2; \
        TEST_OVERDRAW_FINALIZE \
    } while (0)

#define TEST_OVERDRAW_3(EXPECTED_OVERDRAWS, CALL1, CALL2, CALL3) \
    do { \
        TEST_OVERDRAW_SETUP(EXPECTED_OVERDRAWS) \
        context2d()->CALL1; \
        context2d()->CALL2; \
        context2d()->CALL3; \
        TEST_OVERDRAW_FINALIZE \
    } while (0)

#define TEST_OVERDRAW_4(EXPECTED_OVERDRAWS, CALL1, CALL2, CALL3, CALL4) \
    do { \
        TEST_OVERDRAW_SETUP(EXPECTED_OVERDRAWS) \
        context2d()->CALL1; \
        context2d()->CALL2; \
        context2d()->CALL3; \
        context2d()->CALL4; \
        TEST_OVERDRAW_FINALIZE \
    } while (0)

//============================================================================

class AssertNoFallback : public RecordingImageBufferFallbackSurfaceFactory {
public:
    static PassOwnPtr<AssertNoFallback> create() { return adoptPtr(new AssertNoFallback); }

    PassOwnPtr<ImageBufferSurface> createSurface(const IntSize&, OpacityMode) override
    {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    virtual ~AssertNoFallback() { }
};

//============================================================================

TEST_F(CanvasRenderingContext2DTest, detectOverdrawWithFillRect)
{
    createContext(NonOpaque);

    TEST_OVERDRAW_1(1, fillRect(-1, -1, 12, 12));
    TEST_OVERDRAW_1(1, fillRect(0, 0, 10, 10));
    TEST_OVERDRAW_1(0, strokeRect(0, 0, 10, 10)); // stroking instead of filling does not overwrite
    TEST_OVERDRAW_2(0, setGlobalAlpha(0.5f), fillRect(0, 0, 10, 10));
    TEST_OVERDRAW_1(0, fillRect(0, 0, 9, 9));
    TEST_OVERDRAW_2(0, translate(1, 1), fillRect(0, 0, 10, 10));
    TEST_OVERDRAW_2(1, translate(1, 1), fillRect(-1, -1, 10, 10));
    TEST_OVERDRAW_2(1, setFillStyle(m_opaqueGradient), fillRect(0, 0, 10, 10));
    TEST_OVERDRAW_2(0, setFillStyle(m_alphaGradient), fillRect(0, 0, 10, 10));
    TEST_OVERDRAW_3(0, setGlobalAlpha(0.5), setFillStyle(m_opaqueGradient), fillRect(0, 0, 10, 10));
    TEST_OVERDRAW_3(1, setGlobalAlpha(0.5f), setGlobalCompositeOperation(String("copy")), fillRect(0, 0, 10, 10));
    TEST_OVERDRAW_2(1, setGlobalCompositeOperation(String("copy")), fillRect(0, 0, 9, 9));
    TEST_OVERDRAW_3(0, rect(0, 0, 5, 5), clip(), fillRect(0, 0, 10, 10));
    TEST_OVERDRAW_4(0, rect(0, 0, 5, 5), clip(), setGlobalCompositeOperation(String("copy")), fillRect(0, 0, 10, 10));
}

TEST_F(CanvasRenderingContext2DTest, detectOverdrawWithClearRect)
{
    createContext(NonOpaque);

    TEST_OVERDRAW_1(1, clearRect(0, 0, 10, 10));
    TEST_OVERDRAW_1(0, clearRect(0, 0, 9, 9));
    TEST_OVERDRAW_2(1, setGlobalAlpha(0.5f), clearRect(0, 0, 10, 10));
    TEST_OVERDRAW_2(1, setFillStyle(m_alphaGradient), clearRect(0, 0, 10, 10));
    TEST_OVERDRAW_2(0, translate(1, 1), clearRect(0, 0, 10, 10));
    TEST_OVERDRAW_2(1, translate(1, 1), clearRect(-1, -1, 10, 10));
    TEST_OVERDRAW_2(1, setGlobalCompositeOperation(String("destination-in")), clearRect(0, 0, 10, 10)); // composite op ignored
    TEST_OVERDRAW_3(0, rect(0, 0, 5, 5), clip(), clearRect(0, 0, 10, 10));
}

TEST_F(CanvasRenderingContext2DTest, detectOverdrawWithDrawImage)
{
    createContext(NonOpaque);
    NonThrowableExceptionState exceptionState;

    TEST_OVERDRAW_1(1, drawImage(&m_opaqueBitmap, 0, 0, 10, 10, 0, 0, 10, 10, exceptionState));
    EXPECT_FALSE(exceptionState.hadException());
    TEST_OVERDRAW_1(1, drawImage(&m_opaqueBitmap, 0, 0, 1, 1, 0, 0, 10, 10, exceptionState));
    EXPECT_FALSE(exceptionState.hadException());
    TEST_OVERDRAW_2(0, setGlobalAlpha(0.5f), drawImage(&m_opaqueBitmap, 0, 0, 10, 10, 0, 0, 10, 10, exceptionState));
    EXPECT_FALSE(exceptionState.hadException());
    TEST_OVERDRAW_1(0, drawImage(&m_alphaBitmap, 0, 0, 10, 10, 0, 0, 10, 10, exceptionState));
    EXPECT_FALSE(exceptionState.hadException());
    TEST_OVERDRAW_2(0, setGlobalAlpha(0.5f), drawImage(&m_alphaBitmap, 0, 0, 10, 10, 0, 0, 10, 10, exceptionState));
    EXPECT_FALSE(exceptionState.hadException());
    TEST_OVERDRAW_1(0, drawImage(&m_opaqueBitmap, 0, 0, 10, 10, 1, 0, 10, 10, exceptionState));
    EXPECT_FALSE(exceptionState.hadException());
    TEST_OVERDRAW_1(0, drawImage(&m_opaqueBitmap, 0, 0, 10, 10, 0, 0, 9, 9, exceptionState));
    EXPECT_FALSE(exceptionState.hadException());
    TEST_OVERDRAW_1(1, drawImage(&m_opaqueBitmap, 0, 0, 10, 10, 0, 0, 11, 11, exceptionState));
    EXPECT_FALSE(exceptionState.hadException());
    TEST_OVERDRAW_2(1, translate(-1, 0), drawImage(&m_opaqueBitmap, 0, 0, 10, 10, 1, 0, 10, 10, exceptionState));
    EXPECT_FALSE(exceptionState.hadException());
    TEST_OVERDRAW_2(0, translate(-1, 0), drawImage(&m_opaqueBitmap, 0, 0, 10, 10, 0, 0, 10, 10, exceptionState));
    EXPECT_FALSE(exceptionState.hadException());
    TEST_OVERDRAW_2(0, setFillStyle(m_opaqueGradient), drawImage(&m_alphaBitmap, 0, 0, 10, 10, 0, 0, 10, 10, exceptionState)); // fillStyle ignored by drawImage
    EXPECT_FALSE(exceptionState.hadException());
    TEST_OVERDRAW_2(1, setFillStyle(m_alphaGradient), drawImage(&m_opaqueBitmap, 0, 0, 10, 10, 0, 0, 10, 10, exceptionState)); // fillStyle ignored by drawImage
    EXPECT_FALSE(exceptionState.hadException());
    TEST_OVERDRAW_2(1, setGlobalCompositeOperation(String("copy")), drawImage(&m_opaqueBitmap, 0, 0, 10, 10, 1, 0, 10, 10, exceptionState));
    EXPECT_FALSE(exceptionState.hadException());
    TEST_OVERDRAW_3(0, rect(0, 0, 5, 5), clip(), drawImage(&m_opaqueBitmap, 0, 0, 10, 10, 0, 0, 10, 10, exceptionState));
    EXPECT_FALSE(exceptionState.hadException());
}

TEST_F(CanvasRenderingContext2DTest, detectOverdrawWithPutImageData)
{
    createContext(NonOpaque);

    // Test putImageData
    TEST_OVERDRAW_1(1, putImageData(m_fullImageData.get(), 0, 0));
    TEST_OVERDRAW_1(1, putImageData(m_fullImageData.get(), 0, 0, 0, 0, 10, 10));
    TEST_OVERDRAW_1(0, putImageData(m_fullImageData.get(), 0, 0, 1, 1, 8, 8));
    TEST_OVERDRAW_2(1, setGlobalAlpha(0.5f), putImageData(m_fullImageData.get(), 0, 0)); // alpha has no effect
    TEST_OVERDRAW_1(0, putImageData(m_partialImageData.get(), 0, 0));
    TEST_OVERDRAW_2(1, translate(1, 1), putImageData(m_fullImageData.get(), 0, 0)); // ignores tranforms
    TEST_OVERDRAW_1(0, putImageData(m_fullImageData.get(), 1, 0));
    TEST_OVERDRAW_3(1, rect(0, 0, 5, 5), clip(), putImageData(m_fullImageData.get(), 0, 0)); // ignores clip
}

TEST_F(CanvasRenderingContext2DTest, detectOverdrawWithCompositeOperations)
{
    createContext(NonOpaque);

    // Test composite operators with an opaque rect that covers the entire canvas
    // Note: all the untested composite operations take the same code path as source-in,
    // which assumes that the destination may not be overwritten
    TEST_OVERDRAW_2(1, setGlobalCompositeOperation(String("clear")), fillRect(0, 0, 10, 10));
    TEST_OVERDRAW_2(1, setGlobalCompositeOperation(String("copy")), fillRect(0, 0, 10, 10));
    TEST_OVERDRAW_2(1, setGlobalCompositeOperation(String("source-over")), fillRect(0, 0, 10, 10));
    TEST_OVERDRAW_2(0, setGlobalCompositeOperation(String("source-in")), fillRect(0, 0, 10, 10));
    // Test composite operators with a transparent rect that covers the entire canvas
    TEST_OVERDRAW_3(1, setGlobalAlpha(0.5f), setGlobalCompositeOperation(String("clear")), fillRect(0, 0, 10, 10));
    TEST_OVERDRAW_3(1, setGlobalAlpha(0.5f), setGlobalCompositeOperation(String("copy")), fillRect(0, 0, 10, 10));
    TEST_OVERDRAW_3(0, setGlobalAlpha(0.5f), setGlobalCompositeOperation(String("source-over")), fillRect(0, 0, 10, 10));
    TEST_OVERDRAW_3(0, setGlobalAlpha(0.5f), setGlobalCompositeOperation(String("source-in")), fillRect(0, 0, 10, 10));
    // Test composite operators with an opaque rect that does not cover the entire canvas
    TEST_OVERDRAW_2(0, setGlobalCompositeOperation(String("clear")), fillRect(0, 0, 5, 5));
    TEST_OVERDRAW_2(1, setGlobalCompositeOperation(String("copy")), fillRect(0, 0, 5, 5));
    TEST_OVERDRAW_2(0, setGlobalCompositeOperation(String("source-over")), fillRect(0, 0, 5, 5));
    TEST_OVERDRAW_2(0, setGlobalCompositeOperation(String("source-in")), fillRect(0, 0, 5, 5));
}

TEST_F(CanvasRenderingContext2DTest, NoLayerPromotionByDefault)
{
    createContext(NonOpaque);
    OwnPtr<RecordingImageBufferSurface> surface = adoptPtr(new RecordingImageBufferSurface(IntSize(10, 10), AssertNoFallback::create(), NonOpaque));
    canvasElement().createImageBufferUsingSurface(surface.release());

    EXPECT_FALSE(canvasElement().shouldBeDirectComposited());
}

TEST_F(CanvasRenderingContext2DTest, NoLayerPromotionUnderOverdrawLimit)
{
    createContext(NonOpaque);
    OwnPtr<RecordingImageBufferSurface> surface = adoptPtr(new RecordingImageBufferSurface(IntSize(10, 10), AssertNoFallback::create(), NonOpaque));
    canvasElement().createImageBufferUsingSurface(surface.release());

    context2d()->setGlobalAlpha(0.5f); // To prevent overdraw optimization
    for (int i = 0; i < ExpensiveCanvasHeuristicParameters::ExpensiveOverdrawThreshold - 1; i++) {
        context2d()->fillRect(0, 0, 10, 10);
    }

    EXPECT_FALSE(canvasElement().shouldBeDirectComposited());
}

TEST_F(CanvasRenderingContext2DTest, LayerPromotionOverOverdrawLimit)
{
    createContext(NonOpaque);
    OwnPtr<RecordingImageBufferSurface> surface = adoptPtr(new RecordingImageBufferSurface(IntSize(10, 10), AssertNoFallback::create(), NonOpaque));
    canvasElement().createImageBufferUsingSurface(surface.release());

    context2d()->setGlobalAlpha(0.5f); // To prevent overdraw optimization
    for (int i = 0; i < ExpensiveCanvasHeuristicParameters::ExpensiveOverdrawThreshold; i++) {
        context2d()->fillRect(0, 0, 10, 10);
    }

    EXPECT_TRUE(canvasElement().shouldBeDirectComposited());
}

TEST_F(CanvasRenderingContext2DTest, NoLayerPromotionUnderExpensivePathPointCount)
{
    createContext(NonOpaque);
    OwnPtr<RecordingImageBufferSurface> surface = adoptPtr(new RecordingImageBufferSurface(IntSize(10, 10), AssertNoFallback::create(), NonOpaque));
    canvasElement().createImageBufferUsingSurface(surface.release());

    context2d()->beginPath();
    context2d()->moveTo(7, 5);
    for (int i = 1; i < ExpensiveCanvasHeuristicParameters::ExpensivePathPointCount-1; i++) {
        float angleRad = twoPiFloat * i / (ExpensiveCanvasHeuristicParameters::ExpensivePathPointCount - 1);
        context2d()->lineTo(5 + 2 * cos(angleRad), 5 + 2 * sin(angleRad));
    }
    context2d()->fill();

    EXPECT_FALSE(canvasElement().shouldBeDirectComposited());
}

TEST_F(CanvasRenderingContext2DTest, LayerPromotionOverExpensivePathPointCount)
{
    createContext(NonOpaque);
    OwnPtr<RecordingImageBufferSurface> surface = adoptPtr(new RecordingImageBufferSurface(IntSize(10, 10), AssertNoFallback::create(), NonOpaque));
    canvasElement().createImageBufferUsingSurface(surface.release());

    context2d()->beginPath();
    context2d()->moveTo(7, 5);
    for (int i = 1; i < ExpensiveCanvasHeuristicParameters::ExpensivePathPointCount + 1; i++) {
        float angleRad = twoPiFloat * i / (ExpensiveCanvasHeuristicParameters::ExpensivePathPointCount + 1);
        context2d()->lineTo(5 + 2 * cos(angleRad), 5 + 2 * sin(angleRad));
    }
    context2d()->fill();

    EXPECT_TRUE(canvasElement().shouldBeDirectComposited());
}

TEST_F(CanvasRenderingContext2DTest, LayerPromotionWhenPathIsConcave)
{
    createContext(NonOpaque);
    OwnPtr<RecordingImageBufferSurface> surface = adoptPtr(new RecordingImageBufferSurface(IntSize(10, 10), AssertNoFallback::create(), NonOpaque));
    canvasElement().createImageBufferUsingSurface(surface.release());

    context2d()->beginPath();
    context2d()->moveTo(1, 1);
    context2d()->lineTo(5, 5);
    context2d()->lineTo(9, 1);
    context2d()->lineTo(5, 9);
    context2d()->fill();

    if (ExpensiveCanvasHeuristicParameters::ConcavePathsAreExpensive) {
        EXPECT_TRUE(canvasElement().shouldBeDirectComposited());
    } else {
        EXPECT_FALSE(canvasElement().shouldBeDirectComposited());
    }
}

TEST_F(CanvasRenderingContext2DTest, NoLayerPromotionWithRectangleClip)
{
    createContext(NonOpaque);
    OwnPtr<RecordingImageBufferSurface> surface = adoptPtr(new RecordingImageBufferSurface(IntSize(10, 10), AssertNoFallback::create(), NonOpaque));
    canvasElement().createImageBufferUsingSurface(surface.release());

    context2d()->beginPath();
    context2d()->rect(1, 1, 2, 2);
    context2d()->clip();
    context2d()->fillRect(0, 0, 4, 4);

    EXPECT_FALSE(canvasElement().shouldBeDirectComposited());
}

TEST_F(CanvasRenderingContext2DTest, LayerPromotionWithComplexClip)
{
    createContext(NonOpaque);
    OwnPtr<RecordingImageBufferSurface> surface = adoptPtr(new RecordingImageBufferSurface(IntSize(10, 10), AssertNoFallback::create(), NonOpaque));
    canvasElement().createImageBufferUsingSurface(surface.release());

    context2d()->beginPath();
    context2d()->moveTo(1, 1);
    context2d()->lineTo(5, 5);
    context2d()->lineTo(9, 1);
    context2d()->lineTo(5, 9);
    context2d()->clip();
    context2d()->fillRect(0, 0, 4, 4);

    if (ExpensiveCanvasHeuristicParameters::ComplexClipsAreExpensive) {
        EXPECT_TRUE(canvasElement().shouldBeDirectComposited());
    } else {
        EXPECT_FALSE(canvasElement().shouldBeDirectComposited());
    }
}

TEST_F(CanvasRenderingContext2DTest, LayerPromotionWithBlurredShadow)
{
    createContext(NonOpaque);
    OwnPtr<RecordingImageBufferSurface> surface = adoptPtr(new RecordingImageBufferSurface(IntSize(10, 10), AssertNoFallback::create(), NonOpaque));
    canvasElement().createImageBufferUsingSurface(surface.release());

    context2d()->setShadowColor(String("red"));
    context2d()->setShadowBlur(1.0f);
    context2d()->fillRect(1, 1, 1, 1);

    if (ExpensiveCanvasHeuristicParameters::BlurredShadowsAreExpensive) {
        EXPECT_TRUE(canvasElement().shouldBeDirectComposited());
    } else {
        EXPECT_FALSE(canvasElement().shouldBeDirectComposited());
    }
}

TEST_F(CanvasRenderingContext2DTest, NoLayerPromotionWithSharpShadow)
{
    createContext(NonOpaque);
    OwnPtr<RecordingImageBufferSurface> surface = adoptPtr(new RecordingImageBufferSurface(IntSize(10, 10), AssertNoFallback::create(), NonOpaque));
    canvasElement().createImageBufferUsingSurface(surface.release());

    context2d()->setShadowColor(String("red"));
    context2d()->setShadowOffsetX(1.0f);
    context2d()->fillRect(1, 1, 1, 1);

    EXPECT_FALSE(canvasElement().shouldBeDirectComposited());
}

} // unnamed namespace
