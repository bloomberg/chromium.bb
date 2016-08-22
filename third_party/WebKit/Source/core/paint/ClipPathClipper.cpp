// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/ClipPathClipper.h"

#include "core/layout/svg/LayoutSVGResourceClipper.h"
#include "core/style/ClipPathOperation.h"
#include "platform/graphics/paint/ClipPathRecorder.h"

namespace blink {

ClipPathClipper::ClipPathClipper(
    GraphicsContext& context,
    const LayoutObject& layoutObject,
    const FloatRect& referenceBox,
    const FloatRect& visualOverflowRect,
    const FloatPoint& origin)
    : m_resourceClipper(nullptr)
    , m_clipperState(SVGClipPainter::ClipperNotApplied)
    , m_layoutObject(layoutObject)
    , m_context(context)
{
    DCHECK(layoutObject.styleRef().clipPath());
    ClipPathOperation* clipPathOperation = layoutObject.styleRef().clipPath();
    if (clipPathOperation->type() == ClipPathOperation::SHAPE) {
        ShapeClipPathOperation* shape = toShapeClipPathOperation(clipPathOperation);
        if (shape->isValid())
            m_clipPathRecorder.emplace(context, layoutObject, shape->path(referenceBox));
    } else {
        DCHECK_EQ(clipPathOperation->type(), ClipPathOperation::REFERENCE);
        ReferenceClipPathOperation* referenceClipPathOperation = toReferenceClipPathOperation(clipPathOperation);
        Document& document = layoutObject.document();
        // TODO(fs): It doesn't work with forward or external SVG references (https://bugs.webkit.org/show_bug.cgi?id=90405)
        Element* element = document.getElementById(referenceClipPathOperation->fragment());
        if (isSVGClipPathElement(element) && element->layoutObject()) {
            m_resourceClipper = toLayoutSVGResourceClipper(toLayoutSVGResourceContainer(element->layoutObject()));
            // When SVG applies the clip, and the coordinate system is "userspace on use", we must explicitly pass in
            // the offset to have the clip paint in the correct location. When the coordinate system is
            // "object bounding box" the offset should already be accounted for in the visualOverflowRect.
            FloatPoint originTranslation = m_resourceClipper->clipPathUnits() == SVGUnitTypes::kSvgUnitTypeUserspaceonuse ?
                origin : FloatPoint();
            if (!SVGClipPainter(*m_resourceClipper).prepareEffect(layoutObject, referenceBox,
                visualOverflowRect, originTranslation, context, m_clipperState)) {
                // No need to "finish" the clipper if this failed.
                m_resourceClipper = nullptr;
            }
        }
    }
}

ClipPathClipper::~ClipPathClipper()
{
    if (m_resourceClipper)
        SVGClipPainter(*m_resourceClipper).finishEffect(m_layoutObject, m_context, m_clipperState);
}

} // namespace blink
