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
#include "core/layout/LayoutObject.h"
#include "core/svg/SVGElementRareData.h"
#include "core/svg/SVGMatrixTearOff.h"
#include "core/svg/SVGRectTearOff.h"
#include "platform/transforms/AffineTransform.h"

namespace blink {

SVGGraphicsElement::SVGGraphicsElement(const QualifiedName& tag_name,
                                       Document& document,
                                       ConstructionType construction_type)
    : SVGElement(tag_name, document, construction_type),
      SVGTests(this),
      transform_(SVGAnimatedTransformList::Create(this,
                                                  SVGNames::transformAttr,
                                                  CSSPropertyTransform)) {
  AddToPropertyMap(transform_);
}

SVGGraphicsElement::~SVGGraphicsElement() {}

DEFINE_TRACE(SVGGraphicsElement) {
  visitor->Trace(transform_);
  SVGElement::Trace(visitor);
  SVGTests::Trace(visitor);
}

static bool IsViewportElement(const Element& element) {
  return (isSVGSVGElement(element) || isSVGSymbolElement(element) ||
          isSVGForeignObjectElement(element) || isSVGImageElement(element));
}

AffineTransform SVGGraphicsElement::ComputeCTM(
    SVGElement::CTMScope mode,
    const SVGGraphicsElement* ancestor) const {
  AffineTransform ctm;
  bool done = false;

  for (const Element* current_element = this; current_element && !done;
       current_element = current_element->ParentOrShadowHostElement()) {
    if (!current_element->IsSVGElement())
      break;

    ctm = ToSVGElement(current_element)
              ->LocalCoordinateSpaceTransform(mode)
              .Multiply(ctm);

    switch (mode) {
      case kNearestViewportScope:
        // Stop at the nearest viewport ancestor.
        done = current_element != this && IsViewportElement(*current_element);
        break;
      case kAncestorScope:
        // Stop at the designated ancestor.
        done = current_element == ancestor;
        break;
      default:
        DCHECK_EQ(mode, kScreenScope);
        break;
    }
  }
  return ctm;
}

SVGMatrixTearOff* SVGGraphicsElement::getCTM() {
  GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheetsForNode(this);

  return SVGMatrixTearOff::Create(ComputeCTM(kNearestViewportScope));
}

SVGMatrixTearOff* SVGGraphicsElement::getScreenCTM() {
  GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheetsForNode(this);

  return SVGMatrixTearOff::Create(ComputeCTM(kScreenScope));
}

void SVGGraphicsElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableStylePropertySet* style) {
  if (name == SVGNames::transformAttr) {
    AddPropertyToPresentationAttributeStyle(
        style, CSSPropertyTransform, transform_->CurrentValue()->CssValue());
    return;
  }
  SVGElement::CollectStyleForPresentationAttribute(name, value, style);
}

AffineTransform* SVGGraphicsElement::AnimateMotionTransform() {
  return EnsureSVGRareData()->AnimateMotionTransform();
}

void SVGGraphicsElement::SvgAttributeChanged(const QualifiedName& attr_name) {
  // Reattach so the isValid() check will be run again during layoutObject
  // creation.
  if (SVGTests::IsKnownAttribute(attr_name)) {
    SVGElement::InvalidationGuard invalidation_guard(this);
    LazyReattachIfAttached();
    return;
  }

  if (attr_name == SVGNames::transformAttr) {
    SVGElement::InvalidationGuard invalidation_guard(this);
    InvalidateSVGPresentationAttributeStyle();
    // TODO(fs): The InvalidationGuard will make sure all instances are
    // invalidated, but the style recalc will propagate to instances too. So
    // there is some redundant operations being performed here. Could we get
    // away with removing the InvalidationGuard?
    SetNeedsStyleRecalc(kLocalStyleChange,
                        StyleChangeReasonForTracing::FromAttribute(attr_name));
    if (LayoutObject* object = GetLayoutObject())
      MarkForLayoutAndParentResourceInvalidation(object);
    return;
  }

  SVGElement::SvgAttributeChanged(attr_name);
}

SVGElement* SVGGraphicsElement::nearestViewportElement() const {
  for (Element* current = ParentOrShadowHostElement(); current;
       current = current->ParentOrShadowHostElement()) {
    if (IsViewportElement(*current))
      return ToSVGElement(current);
  }

  return nullptr;
}

SVGElement* SVGGraphicsElement::farthestViewportElement() const {
  SVGElement* farthest = 0;
  for (Element* current = ParentOrShadowHostElement(); current;
       current = current->ParentOrShadowHostElement()) {
    if (IsViewportElement(*current))
      farthest = ToSVGElement(current);
  }
  return farthest;
}

FloatRect SVGGraphicsElement::GetBBox() {
  DCHECK(GetLayoutObject());
  return GetLayoutObject()->ObjectBoundingBox();
}

SVGRectTearOff* SVGGraphicsElement::getBBoxFromJavascript() {
  GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();

  // FIXME: Eventually we should support getBBox for detached elements.
  FloatRect boundingBox;
  if (GetLayoutObject())
    boundingBox = GetBBox();
  return SVGRectTearOff::Create(SVGRect::Create(boundingBox), 0,
                                kPropertyIsNotAnimVal);
}

}  // namespace blink
