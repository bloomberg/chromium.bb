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

#ifndef SVGFEColorMatrixElement_h
#define SVGFEColorMatrixElement_h

#include "core/svg/SVGAnimatedEnumeration.h"
#include "core/svg/SVGAnimatedNumberList.h"
#include "core/svg/SVGFilterPrimitiveStandardAttributes.h"
#include "platform/graphics/filters/FEColorMatrix.h"
#include "platform/heap/Handle.h"

namespace blink {

template <>
const SVGEnumerationStringEntries& GetStaticStringEntries<ColorMatrixType>();

class SVGFEColorMatrixElement final
    : public SVGFilterPrimitiveStandardAttributes {
  DEFINE_WRAPPERTYPEINFO();

 public:
  DECLARE_NODE_FACTORY(SVGFEColorMatrixElement);

  SVGAnimatedNumberList* values() { return values_.Get(); }
  SVGAnimatedString* in1() { return in1_.Get(); }
  SVGAnimatedEnumeration<ColorMatrixType>* type() { return type_.Get(); }

  virtual void Trace(blink::Visitor*);

 private:
  explicit SVGFEColorMatrixElement(Document&);

  bool SetFilterEffectAttribute(FilterEffect*, const QualifiedName&) override;
  void SvgAttributeChanged(const QualifiedName&) override;
  FilterEffect* Build(SVGFilterBuilder*, Filter*) override;
  bool TaintsOrigin(bool inputs_taint_origin) const override;

  Member<SVGAnimatedNumberList> values_;
  Member<SVGAnimatedString> in1_;
  Member<SVGAnimatedEnumeration<ColorMatrixType>> type_;
};

}  // namespace blink

#endif  // SVGFEColorMatrixElement_h
