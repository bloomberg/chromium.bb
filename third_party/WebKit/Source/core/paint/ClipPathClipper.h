// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ClipPathClipper_h
#define ClipPathClipper_h

#include "core/paint/SVGClipPainter.h"
#include "platform/graphics/paint/ClipPathRecorder.h"
#include "wtf/Optional.h"

namespace blink {

class FloatPoint;
class FloatRect;
class GraphicsContext;
class LayoutSVGResourceClipper;
class LayoutObject;

class ClipPathClipper {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
public:
    ClipPathClipper(
        GraphicsContext&,
        const LayoutObject&,
        const FloatRect& referenceBox,
        const FloatPoint& origin);
    ~ClipPathClipper();

private:
    LayoutSVGResourceClipper* m_resourceClipper;
    Optional<ClipPathRecorder> m_clipPathRecorder;
    SVGClipPainter::ClipperState m_clipperState;
    const LayoutObject& m_layoutObject;
    GraphicsContext& m_context;
};

} // namespace blink

#endif // ClipPathClipper_h
