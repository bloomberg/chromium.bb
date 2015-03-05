/*
 * Copyright (C) 2004, 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2010 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/html/HTMLCanvasElement.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/WrapCanvasContext.h"
#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/html/ImageData.h"
#include "core/html/canvas/Canvas2DContextAttributes.h"
#include "core/html/canvas/CanvasRenderingContext2D.h"
#include "core/html/canvas/WebGL2RenderingContext.h"
#include "core/html/canvas/WebGLContextAttributes.h"
#include "core/html/canvas/WebGLContextEvent.h"
#include "core/html/canvas/WebGLRenderingContext.h"
#include "core/layout/Layer.h"
#include "core/layout/LayoutHTMLCanvas.h"
#include "platform/MIMETypeRegistry.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/BitmapImage.h"
#include "platform/graphics/Canvas2DImageBufferSurface.h"
#include "platform/graphics/GraphicsContextStateSaver.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/RecordingImageBufferSurface.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "platform/graphics/UnacceleratedImageBufferSurface.h"
#include "platform/graphics/gpu/AcceleratedImageBufferSurface.h"
#include "platform/transforms/AffineTransform.h"
#include "public/platform/Platform.h"
#include <math.h>
#include <v8.h>

namespace blink {

using namespace HTMLNames;

namespace {

// These values come from the WhatWG spec.
const int DefaultWidth = 300;
const int DefaultHeight = 150;

// Firefox limits width/height to 32767 pixels, but slows down dramatically before it
// reaches that limit. We limit by area instead, giving us larger maximum dimensions,
// in exchange for a smaller maximum canvas size.
const int MaxCanvasArea = 32768 * 8192; // Maximum canvas area in CSS pixels

//In Skia, we will also limit width/height to 32767.
const int MaxSkiaDim = 32767; // Maximum width/height in CSS pixels.

bool canCreateImageBuffer(const IntSize& size)
{
    if (size.isEmpty())
        return false;
    if (size.width() * size.height() > MaxCanvasArea)
        return false;
    if (size.width() > MaxSkiaDim || size.height() > MaxSkiaDim)
        return false;
    return true;
}

PassRefPtr<Image> createTransparentImage(const IntSize& size)
{
    ASSERT(canCreateImageBuffer(size));
    SkBitmap bitmap;
    bitmap.allocN32Pixels(size.width(), size.height());
    bitmap.eraseColor(SK_ColorTRANSPARENT);
    return BitmapImage::create(NativeImageSkia::create(bitmap));
}

} // namespace

DEFINE_EMPTY_DESTRUCTOR_WILL_BE_REMOVED(CanvasObserver);

inline HTMLCanvasElement::HTMLCanvasElement(Document& document)
    : HTMLElement(canvasTag, document)
    , DocumentVisibilityObserver(document)
    , m_size(DefaultWidth, DefaultHeight)
    , m_ignoreReset(false)
    , m_accelerationDisabled(false)
    , m_externallyAllocatedMemory(0)
    , m_originClean(true)
    , m_didFailToCreateImageBuffer(false)
    , m_imageBufferIsClear(false)
{
    setHasCustomStyleCallbacks();
}

DEFINE_NODE_FACTORY(HTMLCanvasElement)

HTMLCanvasElement::~HTMLCanvasElement()
{
    v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(-m_externallyAllocatedMemory);
#if !ENABLE(OILPAN)
    for (CanvasObserver* canvasObserver : m_observers)
        canvasObserver->canvasDestroyed(this);
    // Ensure these go away before the ImageBuffer.
    m_contextStateSaver.clear();
    m_context.clear();
#endif
}

void HTMLCanvasElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == widthAttr || name == heightAttr)
        reset();
    HTMLElement::parseAttribute(name, value);
}

LayoutObject* HTMLCanvasElement::createRenderer(const LayoutStyle& style)
{
    LocalFrame* frame = document().frame();
    if (frame && frame->script().canExecuteScripts(NotAboutToExecuteScript))
        return new LayoutHTMLCanvas(this);
    return HTMLElement::createRenderer(style);
}

void HTMLCanvasElement::didRecalcStyle(StyleRecalcChange)
{
    SkPaint::FilterLevel filterLevel = computedStyle()->imageRendering() == ImageRenderingPixelated ? SkPaint::kNone_FilterLevel : SkPaint::kLow_FilterLevel;
    if (is3D()) {
        toWebGLRenderingContextBase(m_context.get())->setFilterLevel(filterLevel);
        setNeedsCompositingUpdate();
    } else if (hasImageBuffer()) {
        m_imageBuffer->setFilterLevel(filterLevel);
    }
}

Node::InsertionNotificationRequest HTMLCanvasElement::insertedInto(ContainerNode* node)
{
    setIsInCanvasSubtree(true);
    return HTMLElement::insertedInto(node);
}

void HTMLCanvasElement::addObserver(CanvasObserver* observer)
{
    m_observers.add(observer);
}

void HTMLCanvasElement::removeObserver(CanvasObserver* observer)
{
    m_observers.remove(observer);
}

void HTMLCanvasElement::setHeight(int value)
{
    setIntegralAttribute(heightAttr, value);
}

void HTMLCanvasElement::setWidth(int value)
{
    setIntegralAttribute(widthAttr, value);
}

ScriptValue HTMLCanvasElement::getContext(ScriptState* scriptState, const String& type, const CanvasContextCreationAttributes& attributes)
{
    CanvasRenderingContext2DOrWebGLRenderingContext result;
    getContext(type, attributes, result);
    if (result.isNull()) {
        return ScriptValue::createNull(scriptState);
    }

    if (result.isCanvasRenderingContext2D()) {
        return wrapCanvasContext(scriptState, this, result.getAsCanvasRenderingContext2D());
    }

    ASSERT(result.isWebGLRenderingContext());
    return wrapCanvasContext(scriptState, this, result.getAsWebGLRenderingContext());
}

void HTMLCanvasElement::getContext(const String& type, const CanvasContextCreationAttributes& attributes, CanvasRenderingContext2DOrWebGLRenderingContext& result)
{
    // A Canvas can either be "2D" or "webgl" but never both. If you request a 2D canvas and the existing
    // context is already 2D, just return that. If the existing context is WebGL, then destroy it
    // before creating a new 2D context. Vice versa when requesting a WebGL canvas. Requesting a
    // context with any other type string will destroy any existing context.
    enum ContextType {
        // Do not change assigned numbers of existing items: add new features to the end of the list.
        Context2d = 0,
        ContextExperimentalWebgl = 2,
        ContextWebgl = 3,
        ContextWebgl2 = 4,
        ContextTypeCount,
    };

    // FIXME - The code depends on the context not going away once created, to prevent JS from
    // seeing a dangling pointer. So for now we will disallow the context from being changed
    // once it is created.
    if (type == "2d") {
        if (m_context && !m_context->is2d())
            return;
        if (!m_context) {
            blink::Platform::current()->histogramEnumeration("Canvas.ContextType", Context2d, ContextTypeCount);

            m_context = CanvasRenderingContext2D::create(this, attributes, document());
            setNeedsCompositingUpdate();
        }
        result.setCanvasRenderingContext2D(static_cast<CanvasRenderingContext2D*>(m_context.get()));
        return;
    }

    // Accept the the provisional "experimental-webgl" or official "webgl" context ID.
    ContextType contextType = Context2d;
    bool is3dContext = true;
    if (type == "experimental-webgl")
        contextType = ContextExperimentalWebgl;
    else if (type == "webgl")
        contextType = ContextWebgl;
    else if (type == "webgl2")
        contextType = ContextWebgl2;
    else
        is3dContext = false;

    if (is3dContext) {
        if (!m_context) {
            blink::Platform::current()->histogramEnumeration("Canvas.ContextType", contextType, ContextTypeCount);
            if (contextType == ContextWebgl2) {
                m_context = WebGL2RenderingContext::create(this, attributes);
            } else {
                m_context = WebGLRenderingContext::create(this, attributes);
            }
            LayoutStyle* style = computedStyle();
            if (style && m_context)
                toWebGLRenderingContextBase(m_context.get())->setFilterLevel(style->imageRendering() == ImageRenderingPixelated ? SkPaint::kNone_FilterLevel : SkPaint::kLow_FilterLevel);
            setNeedsCompositingUpdate();
            updateExternallyAllocatedMemory();
        } else if (!m_context->is3d()) {
            dispatchEvent(WebGLContextEvent::create(EventTypeNames::webglcontextcreationerror, false, true, "Canvas has an existing, non-WebGL context"));
            return;
        }
        result.setWebGLRenderingContext(static_cast<WebGLRenderingContext*>(m_context.get()));
    }

    return;
}

bool HTMLCanvasElement::shouldBeDirectComposited() const
{
    return (m_context && m_context->isAccelerated()) || (hasImageBuffer() && buffer()->isExpensiveToPaint());
}

bool HTMLCanvasElement::isPaintable() const
{
    if (!m_context)
        return canCreateImageBuffer(size());
    return buffer();
}

void HTMLCanvasElement::didDraw(const FloatRect& rect)
{
    if (rect.isEmpty())
        return;
    m_imageBufferIsClear = false;
    clearCopiedImage();
    if (renderer())
        renderer()->setMayNeedPaintInvalidation();
    m_dirtyRect.unite(rect);
    if (m_context && m_context->is2d() && hasImageBuffer())
        buffer()->didDraw(rect);
}

void HTMLCanvasElement::didFinalizeFrame()
{
    if (m_dirtyRect.isEmpty())
        return;

    // Propagate the m_dirtyRect accumulated so far to the compositor
    // before restarting with a blank dirty rect.
    FloatRect srcRect(0, 0, size().width(), size().height());
    m_dirtyRect.intersect(srcRect);
    if (LayoutBox* ro = layoutBox()) {
        LayoutRect mappedDirtyRect(enclosingIntRect(mapRect(m_dirtyRect, srcRect, ro->contentBoxRect())));
        // For querying Layer::compositingState()
        // FIXME: is this invalidation using the correct compositing state?
        DisableCompositingQueryAsserts disabler;
        ro->invalidatePaintRectangle(mappedDirtyRect);
    }
    notifyObserversCanvasChanged(m_dirtyRect);
    m_dirtyRect = FloatRect();
}

void HTMLCanvasElement::restoreCanvasMatrixClipStack()
{
    if (m_context && m_context->is2d())
        toCanvasRenderingContext2D(m_context.get())->restoreCanvasMatrixClipStack();
}

void HTMLCanvasElement::doDeferredPaintInvalidation()
{
    ASSERT(!m_dirtyRect.isEmpty());
    if (is3D()) {
        didFinalizeFrame();
    } else {
        ASSERT(hasImageBuffer());
        m_imageBuffer->finalizeFrame(m_dirtyRect);
    }
    ASSERT(m_dirtyRect.isEmpty());
}


void HTMLCanvasElement::notifyObserversCanvasChanged(const FloatRect& rect)
{
    for (CanvasObserver* canvasObserver : m_observers)
        canvasObserver->canvasChanged(this, rect);
}

void HTMLCanvasElement::reset()
{
    if (m_ignoreReset)
        return;

    m_dirtyRect = FloatRect();

    bool ok;
    bool hadImageBuffer = hasImageBuffer();

    int w = getAttribute(widthAttr).toInt(&ok);
    if (!ok || w < 0)
        w = DefaultWidth;

    int h = getAttribute(heightAttr).toInt(&ok);
    if (!ok || h < 0)
        h = DefaultHeight;

    if (m_contextStateSaver) {
        // Reset to the initial graphics context state.
        m_contextStateSaver->restore();
        m_contextStateSaver->save();
    }

    if (m_context && m_context->is2d())
        toCanvasRenderingContext2D(m_context.get())->reset();

    IntSize oldSize = size();
    IntSize newSize(w, h);

    // If the size of an existing buffer matches, we can just clear it instead of reallocating.
    // This optimization is only done for 2D canvases for now.
    if (hadImageBuffer && oldSize == newSize && m_context && m_context->is2d() && !buffer()->isRecording()) {
        if (!m_imageBufferIsClear) {
            m_imageBufferIsClear = true;
            toCanvasRenderingContext2D(m_context.get())->clearRect(0, 0, width(), height());
        }
        return;
    }

    setSurfaceSize(newSize);

    if (m_context && m_context->is3d() && oldSize != size())
        toWebGLRenderingContextBase(m_context.get())->reshape(width(), height());

    if (LayoutObject* renderer = this->renderer()) {
        if (renderer->isCanvas()) {
            if (oldSize != size()) {
                toLayoutHTMLCanvas(renderer)->canvasSizeChanged();
                if (layoutBox() && layoutBox()->hasAcceleratedCompositing())
                    layoutBox()->contentChanged(CanvasChanged);
            }
            if (hadImageBuffer)
                renderer->setShouldDoFullPaintInvalidation();
        }
    }

    for (CanvasObserver* canvasObserver : m_observers)
        canvasObserver->canvasResized(this);
}

bool HTMLCanvasElement::paintsIntoCanvasBuffer() const
{
    ASSERT(m_context);

    if (!m_context->isAccelerated())
        return true;

    if (layoutBox() && layoutBox()->hasAcceleratedCompositing())
        return false;

    return true;
}


void HTMLCanvasElement::paint(GraphicsContext* context, const LayoutRect& r)
{
    // FIXME: crbug.com/438240; there is a bug with the new CSS blending and compositing feature.
    if (!m_context)
        return;
    if (!paintsIntoCanvasBuffer() && !document().printing())
        return;

    m_context->paintRenderingResultsToCanvas(FrontBuffer);
    if (hasImageBuffer()) {
        SkXfermode::Mode compositeOperator = !m_context || m_context->hasAlpha() ? SkXfermode::kSrcOver_Mode : SkXfermode::kSrc_Mode;
        context->drawImageBuffer(buffer(), pixelSnappedIntRect(r), 0, compositeOperator);
    } else {
        // When alpha is false, we should draw to opaque black.
        if (!m_context->hasAlpha())
            context->fillRect(FloatRect(r), Color(0, 0, 0));
    }

    if (is3D() && paintsIntoCanvasBuffer())
        toWebGLRenderingContextBase(m_context.get())->markLayerComposited();
}

bool HTMLCanvasElement::is3D() const
{
    return m_context && m_context->is3d();
}

void HTMLCanvasElement::setSurfaceSize(const IntSize& size)
{
    m_size = size;
    m_didFailToCreateImageBuffer = false;
    discardImageBuffer();
    clearCopiedImage();
    if (m_context && m_context->is2d()) {
        CanvasRenderingContext2D* context2d = toCanvasRenderingContext2D(m_context.get());
        if (context2d->isContextLost()) {
            context2d->didSetSurfaceSize();
        }
    }
}

String HTMLCanvasElement::toEncodingMimeType(const String& mimeType)
{
    String lowercaseMimeType = mimeType.lower();

    // FIXME: Make isSupportedImageMIMETypeForEncoding threadsafe (to allow this method to be used on a worker thread).
    if (mimeType.isNull() || !MIMETypeRegistry::isSupportedImageMIMETypeForEncoding(lowercaseMimeType))
        lowercaseMimeType = "image/png";

    return lowercaseMimeType;
}

const AtomicString HTMLCanvasElement::imageSourceURL() const
{
    return AtomicString(toDataURLInternal("image/png", 0, FrontBuffer));
}

String HTMLCanvasElement::toDataURLInternal(const String& mimeType, const double* quality, SourceDrawingBuffer sourceBuffer) const
{
    if (!isPaintable())
        return String("data:,");

    String encodingMimeType = toEncodingMimeType(mimeType);
    if (!m_context) {
        RefPtrWillBeRawPtr<ImageData> imageData = ImageData::create(m_size);
        return ImageDataBuffer(imageData->size(), imageData->data()->data()).toDataURL(encodingMimeType, quality);
    }

    if (m_context->is3d()) {
        // Get non-premultiplied data because of inaccurate premultiplied alpha conversion of buffer()->toDataURL().
        RefPtrWillBeRawPtr<ImageData> imageData =
            toWebGLRenderingContextBase(m_context.get())->paintRenderingResultsToImageData(sourceBuffer);
        if (imageData)
            return ImageDataBuffer(imageData->size(), imageData->data()->data()).toDataURL(encodingMimeType, quality);
        m_context->paintRenderingResultsToCanvas(sourceBuffer);
    }

    return buffer()->toDataURL(encodingMimeType, quality);
}

String HTMLCanvasElement::toDataURL(const String& mimeType, const ScriptValue& qualityArgument, ExceptionState& exceptionState) const
{
    if (!originClean()) {
        exceptionState.throwSecurityError("Tainted canvases may not be exported.");
        return String();
    }
    double quality;
    double* qualityPtr = nullptr;
    if (!qualityArgument.isEmpty()) {
        v8::Local<v8::Value> v8Value = qualityArgument.v8Value();
        if (v8Value->IsNumber()) {
            quality = v8Value->NumberValue();
            qualityPtr = &quality;
        }
    }
    return toDataURLInternal(mimeType, qualityPtr, BackBuffer);
}

SecurityOrigin* HTMLCanvasElement::securityOrigin() const
{
    return document().securityOrigin();
}

bool HTMLCanvasElement::originClean() const
{
    if (document().settings() && document().settings()->disableReadingFromCanvas())
        return false;
    return m_originClean;
}

bool HTMLCanvasElement::shouldAccelerate(const IntSize& size) const
{
    if (m_context && !m_context->is2d())
        return false;

    if (m_accelerationDisabled)
        return false;

    Settings* settings = document().settings();
    if (!settings || !settings->accelerated2dCanvasEnabled())
        return false;

    // Do not use acceleration for small canvas.
    if (size.width() * size.height() < settings->minimumAccelerated2dCanvasSize())
        return false;

    if (!blink::Platform::current()->canAccelerate2dCanvas())
        return false;

    return true;
}

class UnacceleratedSurfaceFactory : public RecordingImageBufferFallbackSurfaceFactory {
public:
    virtual PassOwnPtr<ImageBufferSurface> createSurface(const IntSize& size, OpacityMode opacityMode)
    {
        return adoptPtr(new UnacceleratedImageBufferSurface(size, opacityMode));
    }

    virtual ~UnacceleratedSurfaceFactory() { }
};

class Accelerated2dSurfaceFactory : public RecordingImageBufferFallbackSurfaceFactory {
public:
    Accelerated2dSurfaceFactory(int msaaSampleCount) : m_msaaSampleCount(msaaSampleCount) { }

    virtual PassOwnPtr<ImageBufferSurface> createSurface(const IntSize& size, OpacityMode opacityMode)
    {
        OwnPtr<ImageBufferSurface> surface = adoptPtr(new Canvas2DImageBufferSurface(size, opacityMode, m_msaaSampleCount));
        if (surface->isValid())
            return surface.release();
        return adoptPtr(new UnacceleratedImageBufferSurface(size, opacityMode));
    }

    virtual ~Accelerated2dSurfaceFactory() { }
private:
    int m_msaaSampleCount;
};

PassOwnPtr<RecordingImageBufferFallbackSurfaceFactory> HTMLCanvasElement::createSurfaceFactory(const IntSize& deviceSize, int* msaaSampleCount) const
{
    *msaaSampleCount = 0;
    OwnPtr<RecordingImageBufferFallbackSurfaceFactory> surfaceFactory;
    if (shouldAccelerate(deviceSize)) {
        if (document().settings())
            *msaaSampleCount = document().settings()->accelerated2dCanvasMSAASampleCount();
        surfaceFactory = adoptPtr(new Accelerated2dSurfaceFactory(*msaaSampleCount));
    } else {
        surfaceFactory = adoptPtr(new UnacceleratedSurfaceFactory());
    }
    return surfaceFactory.release();
}

bool HTMLCanvasElement::shouldUseDisplayList(const IntSize& deviceSize)
{
    if (RuntimeEnabledFeatures::forceDisplayList2dCanvasEnabled())
        return true;

    if (!RuntimeEnabledFeatures::displayList2dCanvasEnabled())
        return false;

    if (shouldAccelerate(deviceSize))
        return false;

    return true;
}

PassOwnPtr<ImageBufferSurface> HTMLCanvasElement::createImageBufferSurface(const IntSize& deviceSize, int* msaaSampleCount)
{
    OpacityMode opacityMode = !m_context || m_context->hasAlpha() ? NonOpaque : Opaque;

    *msaaSampleCount = 0;
    if (is3D()) {
        // If 3d, but the use of the canvas will be for non-accelerated content
        // (such as -webkit-canvas, then then make a non-accelerated
        // ImageBuffer. This means copying the internal Image will require a
        // pixel readback, but that is unavoidable in this case.
        // FIXME: Actually, avoid setting m_accelerationDisabled at all when
        // doing GPU-based rasterization.
        if (m_accelerationDisabled)
            return adoptPtr(new UnacceleratedImageBufferSurface(deviceSize, opacityMode));
        return adoptPtr(new AcceleratedImageBufferSurface(deviceSize, opacityMode));
    }

    OwnPtr<RecordingImageBufferFallbackSurfaceFactory> surfaceFactory = createSurfaceFactory(deviceSize, msaaSampleCount);

    if (shouldUseDisplayList(deviceSize)) {
        OwnPtr<ImageBufferSurface> surface = adoptPtr(new RecordingImageBufferSurface(deviceSize, surfaceFactory.release(), opacityMode));
        if (surface->isValid())
            return surface.release();
        surfaceFactory = createSurfaceFactory(deviceSize, msaaSampleCount); // recreate because previous one was released
    }

    return surfaceFactory->createSurface(deviceSize, opacityMode);
}

void HTMLCanvasElement::createImageBuffer()
{
    createImageBufferInternal(nullptr);
    if (m_didFailToCreateImageBuffer && m_context->is2d())
        toCanvasRenderingContext2D(m_context.get())->loseContext(CanvasRenderingContext2D::SyntheticLostContext);
}

void HTMLCanvasElement::createImageBufferInternal(PassOwnPtr<ImageBufferSurface> externalSurface)
{
    ASSERT(!m_imageBuffer);
    ASSERT(!m_contextStateSaver);

    m_didFailToCreateImageBuffer = true;
    m_imageBufferIsClear = true;

    if (!canCreateImageBuffer(size()))
        return;

    int msaaSampleCount = 0;
    OwnPtr<ImageBufferSurface> surface;
    if (externalSurface) {
        surface = externalSurface;
    } else {
        surface = createImageBufferSurface(size(), &msaaSampleCount);
    }
    m_imageBuffer = ImageBuffer::create(surface.release());
    if (!m_imageBuffer)
        return;
    m_imageBuffer->setClient(this);

    document().updateRenderTreeIfNeeded();
    LayoutStyle* style = computedStyle();
    m_imageBuffer->setFilterLevel((style && (style->imageRendering() == ImageRenderingPixelated)) ? SkPaint::kNone_FilterLevel : SkPaint::kLow_FilterLevel);

    m_didFailToCreateImageBuffer = false;

    updateExternallyAllocatedMemory();

    if (is3D()) {
        // Early out for WebGL canvases
        return;
    }

    m_imageBuffer->setClient(this);
    m_imageBuffer->context()->setShouldClampToSourceRect(false);
    m_imageBuffer->context()->disableAntialiasingOptimizationForHairlineImages();
    m_imageBuffer->context()->setImageInterpolationQuality(CanvasDefaultInterpolationQuality);
    // Enabling MSAA overrides a request to disable antialiasing. This is true regardless of whether the
    // rendering mode is accelerated or not. For consistency, we don't want to apply AA in accelerated
    // canvases but not in unaccelerated canvases.
    if (!msaaSampleCount && document().settings() && !document().settings()->antialiased2dCanvasEnabled())
        m_imageBuffer->context()->setShouldAntialias(false);
    // GraphicsContext's defaults don't always agree with the 2d canvas spec.
    // See CanvasRenderingContext2D::State::State() for more information.
    m_imageBuffer->context()->setMiterLimit(10);
    m_imageBuffer->context()->setStrokeThickness(1);
#if ENABLE(ASSERT)
    m_imageBuffer->context()->disableDestructionChecks(); // 2D canvas is allowed to leave context in an unfinalized state.
#endif
    m_contextStateSaver = adoptPtr(new GraphicsContextStateSaver(*m_imageBuffer->context()));

    if (m_context)
        setNeedsCompositingUpdate();
}

void HTMLCanvasElement::notifySurfaceInvalid()
{
    if (m_context && m_context->is2d()) {
        CanvasRenderingContext2D* context2d = toCanvasRenderingContext2D(m_context.get());
        context2d->loseContext(CanvasRenderingContext2D::RealLostContext);
    }
}

DEFINE_TRACE(HTMLCanvasElement)
{
#if ENABLE(OILPAN)
    visitor->trace(m_observers);
    visitor->trace(m_context);
#endif
    DocumentVisibilityObserver::trace(visitor);
    HTMLElement::trace(visitor);
}

void HTMLCanvasElement::updateExternallyAllocatedMemory() const
{
    int bufferCount = 0;
    if (m_imageBuffer) {
        bufferCount++;
        if (m_imageBuffer->isAccelerated()) {
            // The number of internal GPU buffers vary between one (stable
            // non-displayed state) and three (triple-buffered animations).
            // Adding 2 is a pessimistic but relevant estimate.
            // Note: These buffers might be allocated in GPU memory.
            bufferCount += 2;
        }
    }
    if (m_copiedImage)
        bufferCount++;

    // Four bytes per pixel per buffer.
    Checked<intptr_t, RecordOverflow> checkedExternallyAllocatedMemory = 4 * bufferCount;
    if (is3D())
        checkedExternallyAllocatedMemory += toWebGLRenderingContextBase(m_context.get())->externallyAllocatedBytesPerPixel();

    checkedExternallyAllocatedMemory *= width();
    checkedExternallyAllocatedMemory *= height();
    intptr_t externallyAllocatedMemory;
    if (checkedExternallyAllocatedMemory.safeGet(externallyAllocatedMemory) == CheckedState::DidOverflow)
        externallyAllocatedMemory = std::numeric_limits<intptr_t>::max();

    // Subtracting two intptr_t that are known to be positive will never underflow.
    v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(externallyAllocatedMemory - m_externallyAllocatedMemory);
    m_externallyAllocatedMemory = externallyAllocatedMemory;
}

GraphicsContext* HTMLCanvasElement::drawingContext() const
{
    return buffer() ? m_imageBuffer->context() : nullptr;
}

SkCanvas* HTMLCanvasElement::drawingCanvas() const
{
    return buffer() ? m_imageBuffer->canvas() : nullptr;
}

SkCanvas* HTMLCanvasElement::existingDrawingCanvas() const
{
    if (!hasImageBuffer())
        return nullptr;

    return m_imageBuffer->canvas();
}

ImageBuffer* HTMLCanvasElement::buffer() const
{
    ASSERT(m_context);
    if (!hasImageBuffer() && !m_didFailToCreateImageBuffer)
        const_cast<HTMLCanvasElement*>(this)->createImageBuffer();
    return m_imageBuffer.get();
}

void HTMLCanvasElement::createImageBufferUsingSurface(PassOwnPtr<ImageBufferSurface> surface)
{
    discardImageBuffer();
    createImageBufferInternal(surface);
}

void HTMLCanvasElement::ensureUnacceleratedImageBuffer()
{
    ASSERT(m_context);
    if ((hasImageBuffer() && !m_imageBuffer->isAccelerated()) || m_didFailToCreateImageBuffer)
        return;
    discardImageBuffer();
    OpacityMode opacityMode = m_context->hasAlpha() ? NonOpaque : Opaque;
    m_imageBuffer = ImageBuffer::create(size(), opacityMode);
    m_didFailToCreateImageBuffer = !m_imageBuffer;
}

PassRefPtr<Image> HTMLCanvasElement::copiedImage(SourceDrawingBuffer sourceBuffer) const
{
    if (!isPaintable())
        return nullptr;
    if (!m_context)
        return createTransparentImage(size());

    bool needToUpdate = !m_copiedImage;
    // The concept of SourceDrawingBuffer is valid on only WebGL.
    if (m_context->is3d())
        needToUpdate |= m_context->paintRenderingResultsToCanvas(sourceBuffer);
    if (needToUpdate && buffer()) {
        m_copiedImage = buffer()->copyImage(CopyBackingStore, Unscaled);
        updateExternallyAllocatedMemory();
    }
    return m_copiedImage;
}

void HTMLCanvasElement::discardImageBuffer()
{
    m_contextStateSaver.clear(); // uses context owned by m_imageBuffer
    m_imageBuffer.clear();
    m_dirtyRect = FloatRect();
    updateExternallyAllocatedMemory();
}

void HTMLCanvasElement::clearCopiedImage()
{
    if (m_copiedImage) {
        m_copiedImage.clear();
        updateExternallyAllocatedMemory();
    }
}

AffineTransform HTMLCanvasElement::baseTransform() const
{
    ASSERT(hasImageBuffer() && !m_didFailToCreateImageBuffer);
    return m_imageBuffer->baseTransform();
}

void HTMLCanvasElement::didChangeVisibilityState(PageVisibilityState visibility)
{
    if (!m_context)
        return;
    bool hidden = visibility != PageVisibilityStateVisible;
    m_context->setIsHidden(hidden);
    if (hidden) {
        clearCopiedImage();
        if (is3D()) {
            discardImageBuffer();
        }
    }
}

void HTMLCanvasElement::didMoveToNewDocument(Document& oldDocument)
{
    setObservedDocument(document());
    if (m_context)
        m_context->didMoveToNewDocument(&document());
    HTMLElement::didMoveToNewDocument(oldDocument);
}

PassRefPtr<Image> HTMLCanvasElement::getSourceImageForCanvas(SourceImageMode mode, SourceImageStatus* status) const
{
    if (!width() || !height()) {
        *status = ZeroSizeCanvasSourceImageStatus;
        return nullptr;
    }

    if (!isPaintable()) {
        *status = InvalidSourceImageStatus;
        return nullptr;
    }

    if (!m_context) {
        *status = NormalSourceImageStatus;
        return createTransparentImage(size());
    }

    m_imageBuffer->willAccessPixels();

    if (m_context->is3d()) {
        m_context->paintRenderingResultsToCanvas(BackBuffer);
    }

    RefPtr<SkImage> image = buffer()->newImageSnapshot();
    if (image) {
        *status = NormalSourceImageStatus;
        return StaticBitmapImage::create(image.release());
    }

    *status = InvalidSourceImageStatus;
    return nullptr;
}

bool HTMLCanvasElement::wouldTaintOrigin(SecurityOrigin*) const
{
    return !originClean();
}

FloatSize HTMLCanvasElement::sourceSize() const
{
    return FloatSize(width(), height());
}

bool HTMLCanvasElement::isOpaque() const
{
    return m_context && !m_context->hasAlpha();
}

} // blink
