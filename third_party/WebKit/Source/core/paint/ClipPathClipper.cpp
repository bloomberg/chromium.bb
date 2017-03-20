// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/ClipPathClipper.h"

#include "core/dom/ElementTraversal.h"
#include "core/layout/svg/LayoutSVGResourceClipper.h"
#include "core/layout/svg/SVGLayoutSupport.h"
#include "core/layout/svg/SVGResources.h"
#include "core/layout/svg/SVGResourcesCache.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/TransformRecorder.h"
#include "core/style/ClipPathOperation.h"
#include "platform/graphics/paint/ClipPathDisplayItem.h"
#include "platform/graphics/paint/ClipPathRecorder.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"
#include "platform/graphics/paint/PaintController.h"
#include "platform/graphics/paint/PaintRecordBuilder.h"

namespace blink {

namespace {

class SVGClipExpansionCycleHelper {
 public:
  SVGClipExpansionCycleHelper(LayoutSVGResourceClipper& clip) : m_clip(clip) {
    clip.beginClipExpansion();
  }
  ~SVGClipExpansionCycleHelper() { m_clip.endClipExpansion(); }

 private:
  LayoutSVGResourceClipper& m_clip;
};

LayoutSVGResourceClipper* resolveElementReference(
    const LayoutObject& layoutObject,
    const ReferenceClipPathOperation& referenceClipPathOperation) {
  if (layoutObject.isSVGChild()) {
    // The reference will have been resolved in
    // SVGResources::buildResources, so we can just use the LayoutObject's
    // SVGResources.
    SVGResources* resources =
        SVGResourcesCache::cachedResourcesForLayoutObject(&layoutObject);
    return resources ? resources->clipper() : nullptr;
  }
  // TODO(fs): Doesn't work with external SVG references (crbug.com/109212.)
  Node* targetNode = layoutObject.node();
  if (!targetNode)
    return nullptr;
  SVGElement* element =
      referenceClipPathOperation.findElement(targetNode->treeScope());
  if (!isSVGClipPathElement(element) || !element->layoutObject())
    return nullptr;
  return toLayoutSVGResourceClipper(
      toLayoutSVGResourceContainer(element->layoutObject()));
}

}  // namespace

ClipPathClipper::ClipPathClipper(GraphicsContext& context,
                                 ClipPathOperation& clipPathOperation,
                                 const LayoutObject& layoutObject,
                                 const FloatRect& referenceBox,
                                 const FloatPoint& origin)
    : m_resourceClipper(nullptr),
      m_clipperState(ClipperState::NotApplied),
      m_layoutObject(layoutObject),
      m_context(context) {
  if (clipPathOperation.type() == ClipPathOperation::SHAPE) {
    ShapeClipPathOperation& shape = toShapeClipPathOperation(clipPathOperation);
    if (!shape.isValid())
      return;
    m_clipPathRecorder.emplace(context, layoutObject, shape.path(referenceBox));
    m_clipperState = ClipperState::AppliedPath;
  } else {
    DCHECK_EQ(clipPathOperation.type(), ClipPathOperation::REFERENCE);
    m_resourceClipper = resolveElementReference(
        layoutObject, toReferenceClipPathOperation(clipPathOperation));
    if (!m_resourceClipper)
      return;
    // Compute the (conservative) bounds of the clip-path.
    FloatRect clipPathBounds =
        m_resourceClipper->resourceBoundingBox(referenceBox);
    // When SVG applies the clip, and the coordinate system is "userspace on
    // use", we must explicitly pass in the offset to have the clip paint in the
    // correct location. When the coordinate system is "object bounding box" the
    // offset should already be accounted for in the reference box.
    FloatPoint originTranslation;
    if (m_resourceClipper->clipPathUnits() ==
        SVGUnitTypes::kSvgUnitTypeUserspaceonuse) {
      clipPathBounds.moveBy(origin);
      originTranslation = origin;
    }
    if (!prepareEffect(referenceBox, clipPathBounds, originTranslation)) {
      // Indicate there is no cleanup to do.
      m_resourceClipper = nullptr;
      return;
    }
  }
}

ClipPathClipper::~ClipPathClipper() {
  if (m_resourceClipper)
    finishEffect();
}

bool ClipPathClipper::prepareEffect(const FloatRect& targetBoundingBox,
                                    const FloatRect& visualRect,
                                    const FloatPoint& layerPositionOffset) {
  DCHECK_EQ(m_clipperState, ClipperState::NotApplied);
  SECURITY_DCHECK(!m_resourceClipper->needsLayout());

  m_resourceClipper->clearInvalidationMask();

  if (m_resourceClipper->hasCycle())
    return false;

  SVGClipExpansionCycleHelper inClipExpansionChange(*m_resourceClipper);

  AffineTransform animatedLocalTransform =
      toSVGClipPathElement(m_resourceClipper->element())
          ->calculateTransform(SVGElement::IncludeMotionTransform);
  // When drawing a clip for non-SVG elements, the CTM does not include the zoom
  // factor.  In this case, we need to apply the zoom scale explicitly - but
  // only for clips with userSpaceOnUse units (the zoom is accounted for
  // objectBoundingBox-resolved lengths).
  if (!m_layoutObject.isSVG() && m_resourceClipper->clipPathUnits() ==
                                     SVGUnitTypes::kSvgUnitTypeUserspaceonuse) {
    DCHECK(m_resourceClipper->style());
    animatedLocalTransform.scale(m_resourceClipper->style()->effectiveZoom());
  }

  // First, try to apply the clip as a clipPath.
  Path clipPath;
  if (m_resourceClipper->asPath(animatedLocalTransform, targetBoundingBox,
                                clipPath)) {
    AffineTransform positionTransform;
    positionTransform.translate(layerPositionOffset.x(),
                                layerPositionOffset.y());
    clipPath.transform(positionTransform);
    m_clipperState = ClipperState::AppliedPath;
    m_context.getPaintController().createAndAppend<BeginClipPathDisplayItem>(
        m_layoutObject, clipPath);
    return true;
  }

  // Fall back to masking.
  m_clipperState = ClipperState::AppliedMask;

  // Begin compositing the clip mask.
  m_maskClipRecorder.emplace(m_context, m_layoutObject, SkBlendMode::kSrcOver,
                             1, &visualRect);
  {
    if (!drawClipAsMask(targetBoundingBox, visualRect, animatedLocalTransform,
                        layerPositionOffset)) {
      // End the clip mask's compositor.
      m_maskClipRecorder.reset();
      return false;
    }
  }

  // Masked content layer start.
  m_maskContentRecorder.emplace(m_context, m_layoutObject, SkBlendMode::kSrcIn,
                                1, &visualRect);

  return true;
}

bool ClipPathClipper::drawClipAsMask(const FloatRect& targetBoundingBox,
                                     const FloatRect& targetVisualRect,
                                     const AffineTransform& localTransform,
                                     const FloatPoint& layerPositionOffset) {
  if (LayoutObjectDrawingRecorder::useCachedDrawingIfPossible(
          m_context, m_layoutObject, DisplayItem::kSVGClip))
    return true;

  PaintRecordBuilder maskBuilder(targetVisualRect, nullptr, &m_context);
  GraphicsContext& maskContext = maskBuilder.context();
  {
    TransformRecorder recorder(maskContext, m_layoutObject, localTransform);

    // Apply any clip-path clipping this clipPath (nested shape/clipPath.)
    Optional<ClipPathClipper> nestedClipPathClipper;
    if (ClipPathOperation* clipPathOperation =
            m_resourceClipper->styleRef().clipPath()) {
      nestedClipPathClipper.emplace(maskContext, *clipPathOperation,
                                    *m_resourceClipper, targetBoundingBox,
                                    layerPositionOffset);
    }

    {
      AffineTransform contentTransform;
      if (m_resourceClipper->clipPathUnits() ==
          SVGUnitTypes::kSvgUnitTypeObjectboundingbox) {
        contentTransform.translate(targetBoundingBox.x(),
                                   targetBoundingBox.y());
        contentTransform.scaleNonUniform(targetBoundingBox.width(),
                                         targetBoundingBox.height());
      }
      SubtreeContentTransformScope contentTransformScope(contentTransform);

      TransformRecorder contentTransformRecorder(maskContext, m_layoutObject,
                                                 contentTransform);
      maskContext.getPaintController().createAndAppend<DrawingDisplayItem>(
          m_layoutObject, DisplayItem::kSVGClip,
          m_resourceClipper->createPaintRecord());
    }
  }

  LayoutObjectDrawingRecorder drawingRecorder(
      m_context, m_layoutObject, DisplayItem::kSVGClip, targetVisualRect);
  m_context.drawRecord(maskBuilder.endRecording());
  return true;
}

void ClipPathClipper::finishEffect() {
  switch (m_clipperState) {
    case ClipperState::AppliedPath:
      // Path-only clipping, no layers to restore but we need to emit an end to
      // the clip path display item.
      m_context.getPaintController().endItem<EndClipPathDisplayItem>(
          m_layoutObject);
      break;
    case ClipperState::AppliedMask:
      // Transfer content -> clip mask (SrcIn)
      m_maskContentRecorder.reset();

      // Transfer clip mask -> bg (SrcOver)
      m_maskClipRecorder.reset();
      break;
    case ClipperState::NotApplied:
      NOTREACHED();
      break;
  }
}

}  // namespace blink
