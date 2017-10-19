/*
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
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

#ifndef SVGFEDropShadowElement_h
#define SVGFEDropShadowElement_h

#include "core/svg/SVGAnimatedNumber.h"
#include "core/svg/SVGAnimatedNumberOptionalNumber.h"
#include "core/svg/SVGFilterPrimitiveStandardAttributes.h"
#include "platform/heap/Handle.h"

namespace blink {

class SVGFEDropShadowElement final
    : public SVGFilterPrimitiveStandardAttributes {
  DEFINE_WRAPPERTYPEINFO();

 public:
  DECLARE_NODE_FACTORY(SVGFEDropShadowElement);

  void setStdDeviation(float std_deviation_x, float std_deviation_y);

  SVGAnimatedNumber* dx() { return dx_.Get(); }
  SVGAnimatedNumber* dy() { return dy_.Get(); }
  SVGAnimatedNumber* stdDeviationX() { return std_deviation_->FirstNumber(); }
  SVGAnimatedNumber* stdDeviationY() { return std_deviation_->SecondNumber(); }
  SVGAnimatedString* in1() { return in1_.Get(); }

  virtual void Trace(blink::Visitor*);

 private:
  explicit SVGFEDropShadowElement(Document&);

  void SvgAttributeChanged(const QualifiedName&) override;
  bool SetFilterEffectAttribute(FilterEffect*, const QualifiedName&) override;
  FilterEffect* Build(SVGFilterBuilder*, Filter*) override;

  static const AtomicString& StdDeviationXIdentifier();
  static const AtomicString& StdDeviationYIdentifier();

  Member<SVGAnimatedNumber> dx_;
  Member<SVGAnimatedNumber> dy_;
  Member<SVGAnimatedNumberOptionalNumber> std_deviation_;
  Member<SVGAnimatedString> in1_;
};

}  // namespace blink

#endif  // SVGFEDropShadowElement_h
