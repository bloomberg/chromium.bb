/*
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
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

#ifndef SVGFEMorphologyElement_h
#define SVGFEMorphologyElement_h

#include "core/svg/SVGAnimatedEnumeration.h"
#include "core/svg/SVGAnimatedNumberOptionalNumber.h"
#include "core/svg/SVGFilterPrimitiveStandardAttributes.h"
#include "platform/graphics/filters/FEMorphology.h"
#include "platform/heap/Handle.h"

namespace blink {

template <>
const SVGEnumerationStringEntries&
GetStaticStringEntries<MorphologyOperatorType>();

class SVGFEMorphologyElement final
    : public SVGFilterPrimitiveStandardAttributes {
  DEFINE_WRAPPERTYPEINFO();

 public:
  DECLARE_NODE_FACTORY(SVGFEMorphologyElement);

  SVGAnimatedNumber* radiusX() { return radius_->FirstNumber(); }
  SVGAnimatedNumber* radiusY() { return radius_->SecondNumber(); }
  SVGAnimatedString* in1() { return in1_.Get(); }
  SVGAnimatedEnumeration<MorphologyOperatorType>* svgOperator() {
    return svg_operator_.Get();
  }

  virtual void Trace(blink::Visitor*);

 private:
  explicit SVGFEMorphologyElement(Document&);

  bool SetFilterEffectAttribute(FilterEffect*, const QualifiedName&) override;
  void SvgAttributeChanged(const QualifiedName&) override;
  FilterEffect* Build(SVGFilterBuilder*, Filter*) override;

  Member<SVGAnimatedNumberOptionalNumber> radius_;
  Member<SVGAnimatedString> in1_;
  Member<SVGAnimatedEnumeration<MorphologyOperatorType>> svg_operator_;
};

}  // namespace blink

#endif  // SVGFEMorphologyElement_h
