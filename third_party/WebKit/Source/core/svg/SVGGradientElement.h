/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
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

#ifndef SVGGradientElement_h
#define SVGGradientElement_h

#include "core/svg/SVGAnimatedEnumeration.h"
#include "core/svg/SVGAnimatedTransformList.h"
#include "core/svg/SVGElement.h"
#include "core/svg/SVGURIReference.h"
#include "core/svg/SVGUnitTypes.h"
#include "core/svg_names.h"
#include "platform/graphics/Gradient.h"
#include "platform/heap/Handle.h"

namespace blink {

struct GradientAttributes;

enum SVGSpreadMethodType {
  kSVGSpreadMethodUnknown = 0,
  kSVGSpreadMethodPad,
  kSVGSpreadMethodReflect,
  kSVGSpreadMethodRepeat
};
template <>
const SVGEnumerationStringEntries&
GetStaticStringEntries<SVGSpreadMethodType>();

class SVGGradientElement : public SVGElement, public SVGURIReference {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(SVGGradientElement);

 public:
  SVGAnimatedTransformList* gradientTransform() const {
    return gradient_transform_.Get();
  }
  SVGAnimatedEnumeration<SVGSpreadMethodType>* spreadMethod() const {
    return spread_method_.Get();
  }
  SVGAnimatedEnumeration<SVGUnitTypes::SVGUnitType>* gradientUnits() const {
    return gradient_units_.Get();
  }

  const SVGGradientElement* ReferencedElement() const;
  void CollectCommonAttributes(GradientAttributes&) const;

  virtual void Trace(blink::Visitor*);

 protected:
  SVGGradientElement(const QualifiedName&, Document&);

  using VisitedSet = HeapHashSet<Member<const SVGGradientElement>>;

  void SvgAttributeChanged(const QualifiedName&) override;

 private:
  bool NeedsPendingResourceHandling() const final { return false; }

  void CollectStyleForPresentationAttribute(const QualifiedName&,
                                            const AtomicString&,
                                            MutableStylePropertySet*) override;

  void ChildrenChanged(const ChildrenChange&) final;

  Vector<Gradient::ColorStop> BuildStops() const;

  Member<SVGAnimatedTransformList> gradient_transform_;
  Member<SVGAnimatedEnumeration<SVGSpreadMethodType>> spread_method_;
  Member<SVGAnimatedEnumeration<SVGUnitTypes::SVGUnitType>> gradient_units_;
};

inline bool IsSVGGradientElement(const SVGElement& element) {
  return element.HasTagName(SVGNames::radialGradientTag) ||
         element.HasTagName(SVGNames::linearGradientTag);
}

DEFINE_SVGELEMENT_TYPE_CASTS_WITH_FUNCTION(SVGGradientElement);

}  // namespace blink

#endif  // SVGGradientElement_h
