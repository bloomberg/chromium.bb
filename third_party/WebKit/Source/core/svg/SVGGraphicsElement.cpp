/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2014 Google, Inc.
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

#include "core/svg/SVGGraphicsElement.h"

#include "core/SVGNames.h"
#include "core/dom/StyleChangeReason.h"
#include "core/frame/FrameView.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/svg/LayoutSVGRoot.h"
#include "core/svg/SVGElementRareData.h"
#include "core/svg/SVGMatrixTearOff.h"
#include "core/svg/SVGRectTearOff.h"
#include "platform/transforms/AffineTransform.h"

namespace blink {

SVGGraphicsElement::SVGGraphicsElement(const QualifiedName& tagName,
                                       Document& document,
                                       ConstructionType constructionType)
    : SVGElement(tagName, document, constructionType),
      SVGTests(this),
      m_transform(SVGAnimatedTransformList::create(this,
                                                   SVGNames::transformAttr,
                                                   CSSPropertyTransform)) {
  addToPropertyMap(m_transform);
}

SVGGraphicsElement::~SVGGraphicsElement() {}

DEFINE_TRACE(SVGGraphicsElement) {
  visitor->trace(m_transform);
  SVGElement::trace(visitor);
  SVGTests::trace(visitor);
}

static bool isViewportElement(const Element& element) {
  return (isSVGSVGElement(element) || isSVGSymbolElement(element) ||
          isSVGForeignObjectElement(element) || isSVGImageElement(element));
}

AffineTransform SVGGraphicsElement::computeCTM(
    SVGElement::CTMScope mode,
    SVGGraphicsElement::StyleUpdateStrategy styleUpdateStrategy,
    const SVGGraphicsElement* ancestor) const {
  if (styleUpdateStrategy == AllowStyleUpdate)
    document().updateStyleAndLayoutIgnorePendingStylesheets();

  AffineTransform ctm;
  bool done = false;

  for (const Element* currentElement = this; currentElement && !done;
       currentElement = currentElement->parentOrShadowHostElement()) {
    if (!currentElement->isSVGElement())
      break;

    ctm = toSVGElement(currentElement)
              ->localCoordinateSpaceTransform()
              .multiply(ctm);

    switch (mode) {
      case NearestViewportScope:
        // Stop at the nearest viewport ancestor.
        done = currentElement != this && isViewportElement(*currentElement);
        break;
      case AncestorScope:
        // Stop at the designated ancestor.
        done = currentElement == ancestor;
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  return ctm;
}

AffineTransform SVGGraphicsElement::getCTM(
    StyleUpdateStrategy styleUpdateStrategy) {
  return computeCTM(NearestViewportScope, styleUpdateStrategy);
}

AffineTransform SVGGraphicsElement::getScreenCTM(
    StyleUpdateStrategy styleUpdateStrategy) {
  if (styleUpdateStrategy == AllowStyleUpdate)
    document().updateStyleAndLayoutIgnorePendingStylesheets();
  TransformationMatrix transform;
  if (LayoutObject* layoutObject = this->layoutObject()) {
    // Adjust for the zoom level factored into CSS coordinates (WK bug #96361).
    transform.scale(1.0 / layoutObject->styleRef().effectiveZoom());

    // Origin in the document. (This, together with the inverse-scale above,
    // performs the same operation as
    // Document::adjustFloatRectForScrollAndAbsoluteZoom, but in transformation
    // matrix form.)
    if (FrameView* view = document().view()) {
      LayoutRect visibleContentRect(view->visibleContentRect());
      transform.translate(-visibleContentRect.x(), -visibleContentRect.y());
    }

    // Apply transforms from our ancestor coordinate space, including any
    // non-SVG ancestor transforms.
    transform.multiply(layoutObject->localToAbsoluteTransform());

    // At the SVG/HTML boundary (aka LayoutSVGRoot), we need to apply the
    // localToBorderBoxTransform to map an element from SVG viewport
    // coordinates to CSS box coordinates.
    if (layoutObject->isSVGRoot()) {
      transform.multiply(
          toLayoutSVGRoot(layoutObject)->localToBorderBoxTransform());
    }
  }
  // Drop any potential non-affine parts, because we're not able to convey that
  // information further anyway until getScreenCTM returns a DOMMatrix (4x4
  // matrix.)
  return transform.toAffineTransform();
}

SVGMatrixTearOff* SVGGraphicsElement::getCTMFromJavascript() {
  return SVGMatrixTearOff::create(getCTM());
}

SVGMatrixTearOff* SVGGraphicsElement::getScreenCTMFromJavascript() {
  return SVGMatrixTearOff::create(getScreenCTM());
}

void SVGGraphicsElement::collectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableStylePropertySet* style) {
  if (name == SVGNames::transformAttr) {
    addPropertyToPresentationAttributeStyle(
        style, CSSPropertyTransform, m_transform->currentValue()->cssValue());
    return;
  }
  SVGElement::collectStyleForPresentationAttribute(name, value, style);
}

AffineTransform* SVGGraphicsElement::animateMotionTransform() {
  return ensureSVGRareData()->animateMotionTransform();
}

void SVGGraphicsElement::svgAttributeChanged(const QualifiedName& attrName) {
  // Reattach so the isValid() check will be run again during layoutObject
  // creation.
  if (SVGTests::isKnownAttribute(attrName)) {
    SVGElement::InvalidationGuard invalidationGuard(this);
    lazyReattachIfAttached();
    return;
  }

  if (attrName == SVGNames::transformAttr) {
    SVGElement::InvalidationGuard invalidationGuard(this);
    invalidateSVGPresentationAttributeStyle();
    // TODO(fs): The InvalidationGuard will make sure all instances are
    // invalidated, but the style recalc will propagate to instances too. So
    // there is some redundant operations being performed here. Could we get
    // away with removing the InvalidationGuard?
    setNeedsStyleRecalc(LocalStyleChange,
                        StyleChangeReasonForTracing::fromAttribute(attrName));
    if (LayoutObject* object = layoutObject())
      markForLayoutAndParentResourceInvalidation(object);
    return;
  }

  SVGElement::svgAttributeChanged(attrName);
}

SVGElement* SVGGraphicsElement::nearestViewportElement() const {
  for (Element* current = parentOrShadowHostElement(); current;
       current = current->parentOrShadowHostElement()) {
    if (isViewportElement(*current))
      return toSVGElement(current);
  }

  return nullptr;
}

SVGElement* SVGGraphicsElement::farthestViewportElement() const {
  SVGElement* farthest = 0;
  for (Element* current = parentOrShadowHostElement(); current;
       current = current->parentOrShadowHostElement()) {
    if (isViewportElement(*current))
      farthest = toSVGElement(current);
  }
  return farthest;
}

FloatRect SVGGraphicsElement::getBBox() {
  document().updateStyleAndLayoutIgnorePendingStylesheets();

  // FIXME: Eventually we should support getBBox for detached elements.
  if (!layoutObject())
    return FloatRect();

  return layoutObject()->objectBoundingBox();
}

SVGRectTearOff* SVGGraphicsElement::getBBoxFromJavascript() {
  return SVGRectTearOff::create(SVGRect::create(getBBox()), 0,
                                PropertyIsNotAnimVal);
}

}  // namespace blink
