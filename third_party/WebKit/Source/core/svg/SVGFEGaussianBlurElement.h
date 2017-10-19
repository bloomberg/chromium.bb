/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
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

#ifndef SVGFEGaussianBlurElement_h
#define SVGFEGaussianBlurElement_h

#include "core/svg/SVGAnimatedNumberOptionalNumber.h"
#include "core/svg/SVGFilterPrimitiveStandardAttributes.h"
#include "platform/heap/Handle.h"

namespace blink {

class SVGFEGaussianBlurElement final
    : public SVGFilterPrimitiveStandardAttributes {
  DEFINE_WRAPPERTYPEINFO();

 public:
  DECLARE_NODE_FACTORY(SVGFEGaussianBlurElement);

  void setStdDeviation(float std_deviation_x, float std_deviation_y);

  SVGAnimatedNumber* stdDeviationX() { return std_deviation_->FirstNumber(); }
  SVGAnimatedNumber* stdDeviationY() { return std_deviation_->SecondNumber(); }
  SVGAnimatedString* in1() { return in1_.Get(); }

  virtual void Trace(blink::Visitor*);

 private:
  explicit SVGFEGaussianBlurElement(Document&);

  void SvgAttributeChanged(const QualifiedName&) override;
  FilterEffect* Build(SVGFilterBuilder*, Filter*) override;

  Member<SVGAnimatedNumberOptionalNumber> std_deviation_;
  Member<SVGAnimatedString> in1_;
};

}  // namespace blink

#endif  // SVGFEGaussianBlurElement_h
