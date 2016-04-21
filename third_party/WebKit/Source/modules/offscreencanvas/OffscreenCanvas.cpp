// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/offscreencanvas/OffscreenCanvas.h"

#include "core/dom/ExceptionCode.h"
#include "core/html/canvas/CanvasContextCreationAttributes.h"
#include "modules/offscreencanvas/OffscreenCanvasRenderingContext.h"
#include "modules/offscreencanvas/OffscreenCanvasRenderingContextFactory.h"
#include "modules/offscreencanvas2d/OffscreenCanvasRenderingContext2D.h"
#include "wtf/MathExtras.h"

namespace blink {

OffscreenCanvas::OffscreenCanvas(const IntSize& size)
    : m_size(size)
{ }

OffscreenCanvas::~OffscreenCanvas()
{ }

OffscreenCanvas* OffscreenCanvas::create(unsigned width, unsigned height)
{
    return new OffscreenCanvas(IntSize(clampTo<int>(width), clampTo<int>(height)));
}

void OffscreenCanvas::setWidth(unsigned width)
{
    m_size.setWidth(clampTo<int>(width));
}

void OffscreenCanvas::setHeight(unsigned height)
{
    m_size.setHeight(clampTo<int>(height));
}

void OffscreenCanvas::setNeutered()
{
    ASSERT(!m_context);
    m_isNeutered = true;
    m_size.setWidth(0);
    m_size.setHeight(0);
}

OffscreenCanvasRenderingContext2D* OffscreenCanvas::getContext(const String& id, const CanvasContextCreationAttributes& attributes, ExceptionState& exceptionState)
{
    if (m_isNeutered) {
        exceptionState.throwDOMException(InvalidStateError, "Cannot get context of a neutered OffscreenCanvas");
        return nullptr;
    }

    OffscreenCanvasRenderingContext::ContextType contextType = OffscreenCanvasRenderingContext::contextTypeFromId(id);

    // Unknown type.
    if (contextType == OffscreenCanvasRenderingContext::ContextTypeCount)
        return nullptr;

    OffscreenCanvasRenderingContextFactory* factory = getRenderingContextFactory(contextType);
    if (!factory)
        return nullptr;

    if (m_context) {
        if (m_context->getContextType() != contextType) {
            factory->onError(this, "OffscreenCanvas has an existing context of a different type");
            return nullptr;
        }
    } else {
        m_context = factory->create(this, attributes);
    }

    // TODO: When there're more than one context type implemented in the future,
    // the return type here should be changed to base class of all Offscreen
    // context types.
    return static_cast<OffscreenCanvasRenderingContext2D*>(m_context.get());
}

ImageBitmap* OffscreenCanvas::transferToImageBitmap(ExceptionState& exceptionState)
{
    if (m_isNeutered) {
        exceptionState.throwDOMException(InvalidStateError, "Cannot transfer an ImageBitmap from a neutered OffscreenCanvas");
        return nullptr;
    }
    if (!m_context) {
        exceptionState.throwDOMException(InvalidStateError, "Cannot transfer an ImageBitmap from an OffscreenCanvas with no context");
        return nullptr;
    }
    ImageBitmap* image = m_context->transferToImageBitmap(exceptionState);
    if (!image) {
        // Undocumented exception (not in spec)
        exceptionState.throwDOMException(V8GeneralError, "Out of memory");
    }
    return image;
}

OffscreenCanvasRenderingContext2D* OffscreenCanvas::renderingContext() const
{
    ASSERT(!m_isNeutered);
    // TODO: When there're more than one context type implemented in the future,
    // the return type here should be changed to base class of all Offscreen
    // context types.
    return static_cast<OffscreenCanvasRenderingContext2D*>(m_context.get());
}

OffscreenCanvas::ContextFactoryVector& OffscreenCanvas::renderingContextFactories()
{
    DEFINE_STATIC_LOCAL(ContextFactoryVector, s_contextFactories, (OffscreenCanvasRenderingContext::ContextTypeCount));
    return s_contextFactories;
}

OffscreenCanvasRenderingContextFactory* OffscreenCanvas::getRenderingContextFactory(int type)
{
    ASSERT(type < OffscreenCanvasRenderingContext::ContextTypeCount);
    return renderingContextFactories()[type].get();
}

void OffscreenCanvas::registerRenderingContextFactory(PassOwnPtr<OffscreenCanvasRenderingContextFactory> renderingContextFactory)
{
    OffscreenCanvasRenderingContext::ContextType type = renderingContextFactory->getContextType();
    ASSERT(type < OffscreenCanvasRenderingContext::ContextTypeCount);
    ASSERT(!renderingContextFactories()[type]);
    renderingContextFactories()[type] = renderingContextFactory;
}

DEFINE_TRACE(OffscreenCanvas)
{
    visitor->trace(m_context);
}

} // namespace blink
