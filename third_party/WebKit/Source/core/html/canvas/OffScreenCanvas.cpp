// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/canvas/OffScreenCanvas.h"

#include "wtf/MathExtras.h"

namespace blink {

OffScreenCanvas* OffScreenCanvas::create(unsigned width, unsigned height)
{
    return new OffScreenCanvas(IntSize(clampTo<int>(width), clampTo<int>(height)));
}

void OffScreenCanvas::setWidth(unsigned width)
{
    m_size.setWidth(clampTo<int>(width));
}

void OffScreenCanvas::setHeight(unsigned height)
{
    m_size.setHeight(clampTo<int>(height));
}

OffScreenCanvas::OffScreenCanvas(const IntSize& size)
    : m_size(size)
{
}

DEFINE_TRACE(OffScreenCanvas)
{
}

} // namespace blink
