// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/offscreencanvas2d/OffscreenCanvasRenderingContext2D.h"

#include "bindings/modules/v8/OffscreenCanvasRenderingContext2DOrWebGLRenderingContextOrWebGL2RenderingContext.h"
#include "core/frame/ImageBitmap.h"
#include "core/frame/Settings.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerSettings.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "platform/graphics/UnacceleratedImageBufferSurface.h"
#include "platform/graphics/gpu/AcceleratedImageBufferSurface.h"
#include "wtf/Assertions.h"

#define UNIMPLEMENTED ASSERT_NOT_REACHED

namespace blink {

OffscreenCanvasRenderingContext2D::~OffscreenCanvasRenderingContext2D()
{
}

OffscreenCanvasRenderingContext2D::OffscreenCanvasRenderingContext2D(ScriptState* scriptState, OffscreenCanvas* canvas, const CanvasContextCreationAttributes& attrs)
    : CanvasRenderingContext(nullptr, canvas, attrs)
{
    ExecutionContext* executionContext = scriptState->getExecutionContext();
    if (executionContext->isDocument()) {
        if (toDocument(executionContext)->settings()->disableReadingFromCanvas())
            canvas->setDisableReadingFromCanvasTrue();
        return;
    }

    WorkerSettings* workerSettings = toWorkerGlobalScope(executionContext)->workerSettings();
    if (workerSettings && workerSettings->disableReadingFromCanvas())
        canvas->setDisableReadingFromCanvasTrue();
}

DEFINE_TRACE(OffscreenCanvasRenderingContext2D)
{
    CanvasRenderingContext::trace(visitor);
    BaseRenderingContext2D::trace(visitor);
}

void OffscreenCanvasRenderingContext2D::commit(ExecutionContext* executionContext)
{
    if (executionContext->isWorkerGlobalScope()) {
        // TODO(xlai): implement commit() on worker thread; currently, do
        // nothing for worker thread. See crbug.com/563858.
        return;
    }
    getOffscreenCanvas()->getOrCreateFrameDispatcher()->dispatchFrame();
}

// BaseRenderingContext2D implementation
bool OffscreenCanvasRenderingContext2D::originClean() const
{
    return getOffscreenCanvas()->originClean();
}

void OffscreenCanvasRenderingContext2D::setOriginTainted()
{
    return getOffscreenCanvas()->setOriginTainted();
}

bool OffscreenCanvasRenderingContext2D::wouldTaintOrigin(CanvasImageSource* source, ExecutionContext* executionContext)
{
    if (executionContext->isWorkerGlobalScope()) {
        // We only support passing in ImageBitmap and OffscreenCanvases as source images
        // in drawImage() or createPattern() in a OffscreenCanvas2d in worker.
        DCHECK(source->isImageBitmap() || source->isOffscreenCanvas());
    }

    return CanvasRenderingContext::wouldTaintOrigin(source, executionContext->getSecurityOrigin());
}

int OffscreenCanvasRenderingContext2D::width() const
{
    return getOffscreenCanvas()->width();
}

int OffscreenCanvasRenderingContext2D::height() const
{
    return getOffscreenCanvas()->height();
}

bool OffscreenCanvasRenderingContext2D::hasImageBuffer() const
{
    return !!m_imageBuffer;
}

static bool shouldAccelerate(IntSize surfaceSize)
{
    if (!isMainThread())
        return false; // Add support on Workers crbug.com/
    return RuntimeEnabledFeatures::accelerated2dCanvasEnabled();
}

ImageBuffer* OffscreenCanvasRenderingContext2D::imageBuffer() const
{
    if (!m_imageBuffer) {
        IntSize surfaceSize(width(), height());
        OpacityMode opacityMode = hasAlpha() ? NonOpaque : Opaque;
        std::unique_ptr<ImageBufferSurface> surface;
        if (shouldAccelerate(surfaceSize)) {
            surface.reset(new AcceleratedImageBufferSurface(surfaceSize, opacityMode));
        }

        if (!surface || !surface->isValid()) {
            surface.reset(new UnacceleratedImageBufferSurface(surfaceSize, opacityMode, InitializeImagePixels));
        }

        OffscreenCanvasRenderingContext2D* nonConstThis = const_cast<OffscreenCanvasRenderingContext2D*>(this);
        nonConstThis->m_imageBuffer = ImageBuffer::create(std::move(surface));

        if (m_needsMatrixClipRestore) {
            restoreMatrixClipStack(m_imageBuffer->canvas());
            nonConstThis->m_needsMatrixClipRestore = false;
        }
    }

    return m_imageBuffer.get();
}

ImageBitmap* OffscreenCanvasRenderingContext2D::transferToImageBitmap(ExceptionState& exceptionState)
{
    if (!imageBuffer())
        return nullptr;
    sk_sp<SkImage> skImage = m_imageBuffer->newSkImageSnapshot(PreferAcceleration, SnapshotReasonTransferToImageBitmap);
    DCHECK(isMainThread() || !skImage->isTextureBacked()); // Acceleration not yet supported in Workers
    RefPtr<StaticBitmapImage> image = StaticBitmapImage::create(std::move(skImage));
    image->setOriginClean(this->originClean());
    m_imageBuffer.reset(); // "Transfer" means no retained buffer
    m_needsMatrixClipRestore = true;
    return ImageBitmap::create(image.release());
}

PassRefPtr<Image> OffscreenCanvasRenderingContext2D::getImage(SnapshotReason reason) const
{
    if (!imageBuffer())
        return nullptr;
    sk_sp<SkImage> skImage = m_imageBuffer->newSkImageSnapshot(PreferAcceleration, reason);
    RefPtr<StaticBitmapImage> image = StaticBitmapImage::create(std::move(skImage));
    return image;
}

void OffscreenCanvasRenderingContext2D::setOffscreenCanvasGetContextResult(OffscreenRenderingContext& result)
{
    result.setOffscreenCanvasRenderingContext2D(this);
}

bool OffscreenCanvasRenderingContext2D::parseColorOrCurrentColor(Color& color, const String& colorString) const
{
    return ::blink::parseColorOrCurrentColor(color, colorString, nullptr);
}

SkCanvas* OffscreenCanvasRenderingContext2D::drawingCanvas() const
{
    ImageBuffer* buffer = imageBuffer();
    if (!buffer)
        return nullptr;
    return imageBuffer()->canvas();
}

SkCanvas* OffscreenCanvasRenderingContext2D::existingDrawingCanvas() const
{
    if (!m_imageBuffer)
        return nullptr;
    return m_imageBuffer->canvas();
}

void OffscreenCanvasRenderingContext2D::disableDeferral(DisableDeferralReason)
{ }

AffineTransform OffscreenCanvasRenderingContext2D::baseTransform() const
{
    if (!m_imageBuffer)
        return AffineTransform(); // identity
    return m_imageBuffer->baseTransform();
}

void OffscreenCanvasRenderingContext2D::didDraw(const SkIRect& dirtyRect)
{ }

bool OffscreenCanvasRenderingContext2D::stateHasFilter()
{
    // TODO: crbug.com/593838 make hasFilter accept nullptr
    // return state().hasFilter(nullptr, nullptr, IntSize(width(), height()), this);
    return false;
}

SkImageFilter* OffscreenCanvasRenderingContext2D::stateGetFilter()
{
    // TODO: make getFilter accept nullptr
    // return state().getFilter(nullptr, nullptr, IntSize(width(), height()), this);
    return nullptr;
}

void OffscreenCanvasRenderingContext2D::validateStateStack()
{
#if ENABLE(ASSERT)
    SkCanvas* skCanvas = existingDrawingCanvas();
    if (skCanvas) {
        ASSERT(static_cast<size_t>(skCanvas->getSaveCount()) == m_stateStack.size());
    }
#endif
}

bool OffscreenCanvasRenderingContext2D::isContextLost() const
{
    return false;
}

bool OffscreenCanvasRenderingContext2D::isPaintable() const
{
    return this->imageBuffer();
}
}
