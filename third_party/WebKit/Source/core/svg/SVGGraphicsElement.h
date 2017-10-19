/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
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

#ifndef SVGGraphicsElement_h
#define SVGGraphicsElement_h

#include "core/CoreExport.h"
#include "core/svg/SVGAnimatedTransformList.h"
#include "core/svg/SVGElement.h"
#include "core/svg/SVGTests.h"
#include "platform/heap/Handle.h"

namespace blink {

class AffineTransform;
class SVGMatrixTearOff;
class SVGRectTearOff;

class CORE_EXPORT SVGGraphicsElement : public SVGElement, public SVGTests {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(SVGGraphicsElement);

 public:
  ~SVGGraphicsElement() override;

  SVGMatrixTearOff* getCTM();
  SVGMatrixTearOff* getScreenCTM();

  SVGElement* nearestViewportElement() const;
  SVGElement* farthestViewportElement() const;

  AffineTransform LocalCoordinateSpaceTransform(CTMScope) const override {
    return CalculateTransform(kIncludeMotionTransform);
  }
  AffineTransform* AnimateMotionTransform() override;

  virtual FloatRect GetBBox();
  SVGRectTearOff* getBBoxFromJavascript();

  bool IsValid() const final { return SVGTests::IsValid(); }

  SVGAnimatedTransformList* transform() { return transform_.Get(); }
  const SVGAnimatedTransformList* transform() const { return transform_.Get(); }

  AffineTransform ComputeCTM(CTMScope mode,
                             const SVGGraphicsElement* ancestor = 0) const;

  virtual void Trace(blink::Visitor*);

 protected:
  SVGGraphicsElement(const QualifiedName&,
                     Document&,
                     ConstructionType = kCreateSVGElement);

  bool SupportsFocus() const override {
    return Element::SupportsFocus() || HasFocusEventListeners();
  }

  void CollectStyleForPresentationAttribute(const QualifiedName&,
                                            const AtomicString&,
                                            MutableStylePropertySet*) override;
  void SvgAttributeChanged(const QualifiedName&) override;

  Member<SVGAnimatedTransformList> transform_;

 private:
  bool IsSVGGraphicsElement() const final { return true; }
};

inline bool IsSVGGraphicsElement(const SVGElement& element) {
  return element.IsSVGGraphicsElement();
}

DEFINE_SVGELEMENT_TYPE_CASTS_WITH_FUNCTION(SVGGraphicsElement);

}  // namespace blink

#endif  // SVGGraphicsElement_h
