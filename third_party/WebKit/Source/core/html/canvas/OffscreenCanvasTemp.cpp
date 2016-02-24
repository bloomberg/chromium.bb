// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/canvas/OffscreenCanvasTemp.h"

#include "wtf/MathExtras.h"

namespace blink {

OffscreenCanvasTemp* OffscreenCanvasTemp::create(unsigned width, unsigned height)
{
    return new OffscreenCanvasTemp(IntSize(clampTo<int>(width), clampTo<int>(height)));
}

void OffscreenCanvasTemp::setWidth(unsigned width)
{
    m_size.setWidth(clampTo<int>(width));
}

void OffscreenCanvasTemp::setHeight(unsigned height)
{
    m_size.setHeight(clampTo<int>(height));
}

OffscreenCanvasTemp::OffscreenCanvasTemp(const IntSize& size)
    : m_size(size)
{
}

DEFINE_TRACE(OffscreenCanvasTemp)
{
}

} // namespace blink
