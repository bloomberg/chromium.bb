/*
 * Copyright (C) 2004, 2005, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 * Copyright (C) 2011 Dirk Schulze <krit@webkit.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "core/layout/svg/LayoutSVGResourceClipper.h"

#include "core/dom/ElementTraversal.h"
#include "core/layout/HitTestResult.h"
#include "core/layout/svg/SVGLayoutSupport.h"
#include "core/paint/PaintInfo.h"
#include "core/svg/SVGGeometryElement.h"
#include "core/svg/SVGUseElement.h"
#include "platform/graphics/paint/PaintRecord.h"
#include "platform/graphics/paint/PaintRecordBuilder.h"
#include "third_party/skia/include/pathops/SkPathOps.h"

namespace blink {

namespace {

enum class ClipStrategy { None, Mask, Path };

ClipStrategy modifyStrategyForClipPath(const ComputedStyle& style,
                                       ClipStrategy strategy) {
  // If the shape in the clip-path gets clipped too then fallback to masking.
  if (strategy != ClipStrategy::Path || !style.clipPath())
    return strategy;
  return ClipStrategy::Mask;
}

ClipStrategy determineClipStrategy(const SVGGraphicsElement& element) {
  const LayoutObject* layoutObject = element.layoutObject();
  if (!layoutObject)
    return ClipStrategy::None;
  const ComputedStyle& style = layoutObject->styleRef();
  if (style.display() == EDisplay::None ||
      style.visibility() != EVisibility::kVisible)
    return ClipStrategy::None;
  ClipStrategy strategy = ClipStrategy::None;
  // Only shapes, paths and texts are allowed for clipping.
  if (layoutObject->isSVGShape()) {
    strategy = ClipStrategy::Path;
  } else if (layoutObject->isSVGText()) {
    // Text requires masking.
    strategy = ClipStrategy::Mask;
  }
  return modifyStrategyForClipPath(style, strategy);
}

ClipStrategy determineClipStrategy(const SVGElement& element) {
  // <use> within <clipPath> have a restricted content model.
  // (https://drafts.fxtf.org/css-masking-1/#ClipPathElement)
  if (isSVGUseElement(element)) {
    const LayoutObject* useLayoutObject = element.layoutObject();
    if (!useLayoutObject ||
        useLayoutObject->styleRef().display() == EDisplay::None)
      return ClipStrategy::None;
    const SVGGraphicsElement* shapeElement =
        toSVGUseElement(element).visibleTargetGraphicsElementForClipping();
    if (!shapeElement)
      return ClipStrategy::None;
    ClipStrategy shapeStrategy = determineClipStrategy(*shapeElement);
    return modifyStrategyForClipPath(useLayoutObject->styleRef(),
                                     shapeStrategy);
  }
  if (!element.isSVGGraphicsElement())
    return ClipStrategy::None;
  return determineClipStrategy(toSVGGraphicsElement(element));
}

bool contributesToClip(const SVGElement& element) {
  return determineClipStrategy(element) != ClipStrategy::None;
}

void pathFromElement(const SVGElement& element, Path& clipPath) {
  if (isSVGGeometryElement(element))
    toSVGGeometryElement(element).toClipPath(clipPath);
  else if (isSVGUseElement(element))
    toSVGUseElement(element).toClipPath(clipPath);
}

}  // namespace

LayoutSVGResourceClipper::LayoutSVGResourceClipper(SVGClipPathElement* node)
    : LayoutSVGResourceContainer(node), m_inClipExpansion(false) {}

LayoutSVGResourceClipper::~LayoutSVGResourceClipper() {}

void LayoutSVGResourceClipper::removeAllClientsFromCache(
    bool markForInvalidation) {
  m_clipContentPath.clear();
  m_cachedPaintRecord.reset();
  m_localClipBounds = FloatRect();
  markAllClientsForInvalidation(markForInvalidation
                                    ? LayoutAndBoundariesInvalidation
                                    : ParentOnlyInvalidation);
}

void LayoutSVGResourceClipper::removeClientFromCache(LayoutObject* client,
                                                     bool markForInvalidation) {
  ASSERT(client);
  markClientForInvalidation(client, markForInvalidation
                                        ? BoundariesInvalidation
                                        : ParentOnlyInvalidation);
}

bool LayoutSVGResourceClipper::calculateClipContentPathIfNeeded() {
  if (!m_clipContentPath.isEmpty())
    return true;

  // If the current clip-path gets clipped itself, we have to fallback to
  // masking.
  if (styleRef().clipPath())
    return false;

  unsigned opCount = 0;
  bool usingBuilder = false;
  SkOpBuilder clipPathBuilder;

  for (const SVGElement& childElement :
       Traversal<SVGElement>::childrenOf(*element())) {
    ClipStrategy strategy = determineClipStrategy(childElement);
    if (strategy == ClipStrategy::None)
      continue;
    if (strategy == ClipStrategy::Mask) {
      m_clipContentPath.clear();
      return false;
    }

    // First clip shape.
    if (m_clipContentPath.isEmpty()) {
      pathFromElement(childElement, m_clipContentPath);
      continue;
    }

    // Multiple shapes require PathOps. In some degenerate cases PathOps can
    // exhibit quadratic behavior, so we cap the number of ops to a reasonable
    // count.
    const unsigned kMaxOps = 42;
    if (++opCount > kMaxOps) {
      m_clipContentPath.clear();
      return false;
    }

    // Second clip shape => start using the builder.
    if (!usingBuilder) {
      clipPathBuilder.add(m_clipContentPath.getSkPath(), kUnion_SkPathOp);
      usingBuilder = true;
    }

    Path subPath;
    pathFromElement(childElement, subPath);

    clipPathBuilder.add(subPath.getSkPath(), kUnion_SkPathOp);
  }

  if (usingBuilder) {
    SkPath resolvedPath;
    clipPathBuilder.resolve(&resolvedPath);
    m_clipContentPath = resolvedPath;
  }

  return true;
}

bool LayoutSVGResourceClipper::asPath(
    const AffineTransform& animatedLocalTransform,
    const FloatRect& referenceBox,
    Path& clipPath) {
  if (!calculateClipContentPathIfNeeded())
    return false;

  clipPath = m_clipContentPath;

  // We are able to represent the clip as a path. Continue with direct clipping,
  // and transform the content to userspace if necessary.
  if (clipPathUnits() == SVGUnitTypes::kSvgUnitTypeObjectboundingbox) {
    AffineTransform transform;
    transform.translate(referenceBox.x(), referenceBox.y());
    transform.scaleNonUniform(referenceBox.width(), referenceBox.height());
    clipPath.transform(transform);
  }

  // Transform path by animatedLocalTransform.
  clipPath.transform(animatedLocalTransform);
  return true;
}

sk_sp<const PaintRecord> LayoutSVGResourceClipper::createPaintRecord() {
  ASSERT(frame());
  if (m_cachedPaintRecord)
    return m_cachedPaintRecord;

  // Using strokeBoundingBox (instead of visualRectInLocalSVGCoordinates) to
  // avoid the intersection with local clips/mask, which may yield incorrect
  // results when mixing objectBoundingBox and userSpaceOnUse units
  // (http://crbug.com/294900).
  FloatRect bounds = strokeBoundingBox();

  PaintRecordBuilder builder(bounds, nullptr, nullptr);
  // Switch to a paint behavior where all children of this <clipPath> will be
  // laid out using special constraints:
  // - fill-opacity/stroke-opacity/opacity set to 1
  // - masker/filter not applied when laying out the children
  // - fill is set to the initial fill paint server (solid, black)
  // - stroke is set to the initial stroke paint server (none)
  PaintInfo info(builder.context(), LayoutRect::infiniteIntRect(),
                 PaintPhaseForeground, GlobalPaintNormalPhase,
                 PaintLayerPaintingRenderingClipPathAsMask |
                     PaintLayerPaintingRenderingResourceSubtree);

  for (const SVGElement& childElement :
       Traversal<SVGElement>::childrenOf(*element())) {
    if (!contributesToClip(childElement))
      continue;
    // Use the LayoutObject of the direct child even if it is a <use>. In that
    // case, we will paint the targeted element indirectly.
    const LayoutObject* layoutObject = childElement.layoutObject();
    layoutObject->paint(info, IntPoint());
  }

  m_cachedPaintRecord = builder.endRecording();
  return m_cachedPaintRecord;
}

void LayoutSVGResourceClipper::calculateLocalClipBounds() {
  // This is a rough heuristic to appraise the clip size and doesn't consider
  // clip on clip.
  for (const SVGElement& childElement :
       Traversal<SVGElement>::childrenOf(*element())) {
    if (!contributesToClip(childElement))
      continue;
    const LayoutObject* layoutObject = childElement.layoutObject();
    m_localClipBounds.unite(layoutObject->localToSVGParentTransform().mapRect(
        layoutObject->visualRectInLocalSVGCoordinates()));
  }
}

bool LayoutSVGResourceClipper::hitTestClipContent(
    const FloatRect& objectBoundingBox,
    const FloatPoint& nodeAtPoint) {
  FloatPoint point = nodeAtPoint;
  if (!SVGLayoutSupport::pointInClippingArea(*this, point))
    return false;

  if (clipPathUnits() == SVGUnitTypes::kSvgUnitTypeObjectboundingbox) {
    AffineTransform transform;
    transform.translate(objectBoundingBox.x(), objectBoundingBox.y());
    transform.scaleNonUniform(objectBoundingBox.width(),
                              objectBoundingBox.height());
    point = transform.inverse().mapPoint(point);
  }

  AffineTransform animatedLocalTransform =
      toSVGClipPathElement(element())->calculateTransform(
          SVGElement::IncludeMotionTransform);
  if (!animatedLocalTransform.isInvertible())
    return false;

  point = animatedLocalTransform.inverse().mapPoint(point);

  for (const SVGElement& childElement :
       Traversal<SVGElement>::childrenOf(*element())) {
    if (!contributesToClip(childElement))
      continue;
    IntPoint hitPoint;
    HitTestResult result(HitTestRequest::SVGClipContent, hitPoint);
    LayoutObject* layoutObject = childElement.layoutObject();
    if (layoutObject->nodeAtFloatPoint(result, point, HitTestForeground))
      return true;
  }
  return false;
}

FloatRect LayoutSVGResourceClipper::resourceBoundingBox(
    const FloatRect& referenceBox) {
  // The resource has not been layouted yet. Return the reference box.
  if (selfNeedsLayout())
    return referenceBox;

  if (m_localClipBounds.isEmpty())
    calculateLocalClipBounds();

  AffineTransform transform =
      toSVGClipPathElement(element())->calculateTransform(
          SVGElement::IncludeMotionTransform);
  if (clipPathUnits() == SVGUnitTypes::kSvgUnitTypeObjectboundingbox) {
    transform.translate(referenceBox.x(), referenceBox.y());
    transform.scaleNonUniform(referenceBox.width(), referenceBox.height());
  }
  return transform.mapRect(m_localClipBounds);
}

}  // namespace blink
