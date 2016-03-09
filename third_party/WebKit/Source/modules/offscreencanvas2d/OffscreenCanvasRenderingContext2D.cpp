// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/offscreencanvas2d/OffscreenCanvasRenderingContext2D.h"

#include "bindings/modules/v8/UnionTypesModules.h"
#include "platform/NotImplemented.h"
#include "wtf/Assertions.h"

#define UNIMPLEMENTED ASSERT_NOT_REACHED

namespace blink {

OffscreenCanvasRenderingContext2D::~OffscreenCanvasRenderingContext2D()
{
}

OffscreenCanvasRenderingContext2D::OffscreenCanvasRenderingContext2D(OffscreenCanvas* canvas, const CanvasContextCreationAttributes& attrs)
    : OffscreenCanvasRenderingContext(canvas)
    , m_hasAlpha(attrs.alpha())
{
}

DEFINE_TRACE(OffscreenCanvasRenderingContext2D)
{
    OffscreenCanvasRenderingContext::trace(visitor);
    BaseRenderingContext2D::trace(visitor);
}

// BaseRenderingContext2D implementation
bool OffscreenCanvasRenderingContext2D::originClean() const
{
    notImplemented();
    return true;
}

void OffscreenCanvasRenderingContext2D::setOriginTainted()
{
    notImplemented();
}

bool OffscreenCanvasRenderingContext2D::wouldTaintOrigin(CanvasImageSource* source)
{
    notImplemented();
    return false;
}

int OffscreenCanvasRenderingContext2D::width() const
{
    return getOffscreenCanvas()->height();
}

int OffscreenCanvasRenderingContext2D::height() const
{
    return getOffscreenCanvas()->width();
}

bool OffscreenCanvasRenderingContext2D::hasImageBuffer() const
{
    notImplemented();
    return false;
}

ImageBuffer* OffscreenCanvasRenderingContext2D::imageBuffer() const
{
    notImplemented();
    return nullptr;
}

bool OffscreenCanvasRenderingContext2D::parseColorOrCurrentColor(Color&, const String& colorString) const
{
    notImplemented();
    return false;
}

SkCanvas* OffscreenCanvasRenderingContext2D::drawingCanvas() const
{
    notImplemented();
    return nullptr;
}

SkCanvas* OffscreenCanvasRenderingContext2D::existingDrawingCanvas() const
{
    notImplemented();
    return nullptr;
}

void OffscreenCanvasRenderingContext2D::disableDeferral(DisableDeferralReason)
{
    notImplemented();
}

AffineTransform OffscreenCanvasRenderingContext2D::baseTransform() const
{
    notImplemented();
    return 0;
}

void OffscreenCanvasRenderingContext2D::didDraw(const SkIRect& dirtyRect)
{
    notImplemented();
}

bool OffscreenCanvasRenderingContext2D::stateHasFilter()
{
    notImplemented();
    return false;
}

SkImageFilter* OffscreenCanvasRenderingContext2D::stateGetFilter()
{
    notImplemented();
    return nullptr;
}

void OffscreenCanvasRenderingContext2D::validateStateStack()
{
    notImplemented();
}

bool OffscreenCanvasRenderingContext2D::isContextLost() const
{
    notImplemented();
    return false;
}

}
