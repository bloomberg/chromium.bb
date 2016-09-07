// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/offscreencanvas/OffscreenCanvas.h"

#include "core/dom/ExceptionCode.h"
#include "core/html/canvas/CanvasContextCreationAttributes.h"
#include "core/html/canvas/CanvasRenderingContext.h"
#include "core/html/canvas/CanvasRenderingContextFactory.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/OffscreenCanvasFrameDispatcherImpl.h"
#include "wtf/MathExtras.h"
#include <memory>

namespace blink {

OffscreenCanvas::OffscreenCanvas(const IntSize& size)
    : m_size(size)
    , m_originClean(true)
{
}

OffscreenCanvas* OffscreenCanvas::create(unsigned width, unsigned height)
{
    return new OffscreenCanvas(IntSize(clampTo<int>(width), clampTo<int>(height)));
}

void OffscreenCanvas::setWidth(unsigned width)
{
    // If this OffscreenCanvas is transferred control by an html canvas,
    // its size is determined by html canvas's size and cannot be resized.
    if (m_canvasId >= 0)
        return;
    m_size.setWidth(clampTo<int>(width));
}

void OffscreenCanvas::setHeight(unsigned height)
{
    // Same comment as above.
    if (m_canvasId >= 0)
        return;
    m_size.setHeight(clampTo<int>(height));
}

void OffscreenCanvas::setNeutered()
{
    ASSERT(!m_context);
    m_isNeutered = true;
    m_size.setWidth(0);
    m_size.setHeight(0);
}

ImageBitmap* OffscreenCanvas::transferToImageBitmap(ExceptionState& exceptionState)
{
    if (m_isNeutered) {
        exceptionState.throwDOMException(InvalidStateError, "Cannot transfer an ImageBitmap from a detached OffscreenCanvas");
        return nullptr;
    }
    if (!m_context) {
        exceptionState.throwDOMException(InvalidStateError, "Cannot transfer an ImageBitmap from an OffscreenCanvas with no context");
        return nullptr;
    }
    ImageBitmap* image = m_context->transferToImageBitmap(exceptionState);
    if (!image) {
        // Undocumented exception (not in spec)
        exceptionState.throwDOMException(V8Error, "Out of memory");
    }
    return image;
}

PassRefPtr<Image> OffscreenCanvas::getSourceImageForCanvas(SourceImageStatus* status, AccelerationHint hint, SnapshotReason reason, const FloatSize&) const
{
    if (!m_context) {
        *status = InvalidSourceImageStatus;
        return nullptr;
    }
    *status = NormalSourceImageStatus;
    return m_context->getImage(hint, reason);
}

bool OffscreenCanvas::isOpaque() const
{
    if (!m_context)
        return false;
    return !m_context->creationAttributes().hasAlpha();
}

CanvasRenderingContext* OffscreenCanvas::getCanvasRenderingContext(ScriptState* scriptState, const String& id, const CanvasContextCreationAttributes& attributes)
{
    CanvasRenderingContext::ContextType contextType = CanvasRenderingContext::contextTypeFromId(id);

    // Unknown type.
    if (contextType == CanvasRenderingContext::ContextTypeCount)
        return nullptr;

    CanvasRenderingContextFactory* factory = getRenderingContextFactory(contextType);
    if (!factory)
        return nullptr;

    if (m_context) {
        if (m_context->getContextType() != contextType) {
            factory->onError(this, "OffscreenCanvas has an existing context of a different type");
            return nullptr;
        }
    } else {
        m_context = factory->create(scriptState, this, attributes);
    }

    return m_context.get();
}

OffscreenCanvas::ContextFactoryVector& OffscreenCanvas::renderingContextFactories()
{
    DEFINE_STATIC_LOCAL(ContextFactoryVector, s_contextFactories, (CanvasRenderingContext::ContextTypeCount));
    return s_contextFactories;
}

CanvasRenderingContextFactory* OffscreenCanvas::getRenderingContextFactory(int type)
{
    ASSERT(type < CanvasRenderingContext::ContextTypeCount);
    return renderingContextFactories()[type].get();
}

void OffscreenCanvas::registerRenderingContextFactory(std::unique_ptr<CanvasRenderingContextFactory> renderingContextFactory)
{
    CanvasRenderingContext::ContextType type = renderingContextFactory->getContextType();
    ASSERT(type < CanvasRenderingContext::ContextTypeCount);
    ASSERT(!renderingContextFactories()[type]);
    renderingContextFactories()[type] = std::move(renderingContextFactory);
}

bool OffscreenCanvas::originClean() const
{
    return m_originClean && !m_disableReadingFromCanvas;
}

bool OffscreenCanvas::isPaintable() const
{
    if (!m_context)
        return ImageBuffer::canCreateImageBuffer(m_size);
    return m_context->isPaintable();
}

OffscreenCanvasFrameDispatcher* OffscreenCanvas::getOrCreateFrameDispatcher()
{
    if (!m_frameDispatcher)
        m_frameDispatcher = wrapUnique(new OffscreenCanvasFrameDispatcherImpl(m_clientId, m_localId, m_nonce, width(), height()));
    return m_frameDispatcher.get();
}

DEFINE_TRACE(OffscreenCanvas)
{
    visitor->trace(m_context);
}

} // namespace blink
