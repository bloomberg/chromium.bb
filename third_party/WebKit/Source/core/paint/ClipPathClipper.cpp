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
        if (!shape->isValid())
            return;
        m_clipPathRecorder.emplace(context, layoutObject, shape->path(referenceBox));
    } else {
        DCHECK_EQ(clipPathOperation->type(), ClipPathOperation::REFERENCE);
        ReferenceClipPathOperation* referenceClipPathOperation = toReferenceClipPathOperation(clipPathOperation);
        // TODO(fs): It doesn't work with forward or external SVG references (https://bugs.webkit.org/show_bug.cgi?id=90405)
        Element* element = layoutObject.document().getElementById(referenceClipPathOperation->fragment());
        if (!isSVGClipPathElement(element) || !element->layoutObject())
            return;
        LayoutSVGResourceClipper* clipper = toLayoutSVGResourceClipper(toLayoutSVGResourceContainer(element->layoutObject()));
        // Compute the (conservative) bounds of the clip-path.
        FloatRect clipPathBounds = clipper->resourceBoundingBox(referenceBox);
        // When SVG applies the clip, and the coordinate system is "userspace on use", we must explicitly pass in
        // the offset to have the clip paint in the correct location. When the coordinate system is
        // "object bounding box" the offset should already be accounted for in the reference box.
        FloatPoint originTranslation;
        if (clipper->clipPathUnits() == SVGUnitTypes::kSvgUnitTypeUserspaceonuse) {
            clipPathBounds.moveBy(origin);
            originTranslation = origin;
        }
        if (!SVGClipPainter(*clipper).prepareEffect(layoutObject, referenceBox,
            clipPathBounds, originTranslation, context, m_clipperState))
            return;
        m_resourceClipper = clipper;
    }
}

ClipPathClipper::~ClipPathClipper()
{
    if (m_resourceClipper)
        SVGClipPainter(*m_resourceClipper).finishEffect(m_layoutObject, m_context, m_clipperState);
}

} // namespace blink
