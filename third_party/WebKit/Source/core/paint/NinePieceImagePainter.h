// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NinePieceImagePainter_h
#define NinePieceImagePainter_h

#include "platform/LayoutUnit.h"
#include "platform/heap/Heap.h"
#include "third_party/skia/include/core/SkXfermode.h"

namespace blink {

class ComputedStyle;
class GraphicsContext;
class LayoutBoxModelObject;
class LayoutRect;
class NinePieceImage;
class BorderImageLength;

class NinePieceImagePainter {
    STACK_ALLOCATED();
public:
    NinePieceImagePainter(LayoutBoxModelObject&);

    bool paint(GraphicsContext*, const LayoutRect&, const ComputedStyle&, const NinePieceImage&, SkXfermode::Mode) const;

private:
    static LayoutUnit computeBorderImageSide(const BorderImageLength& borderSlice, LayoutUnit borderSide, LayoutUnit imageSide, LayoutUnit boxExtent);

    LayoutBoxModelObject& m_layoutObject;
};

} // namespace blink

#endif // NinePieceImagePainter_h
