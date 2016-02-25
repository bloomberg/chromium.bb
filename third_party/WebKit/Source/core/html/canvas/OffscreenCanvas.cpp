// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/canvas/OffscreenCanvas.h"

#include "wtf/MathExtras.h"

namespace blink {

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

OffscreenCanvas::OffscreenCanvas(const IntSize& size)
    : m_size(size)
{
}

DEFINE_TRACE(OffscreenCanvas)
{
}

} // namespace blink
