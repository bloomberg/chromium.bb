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

#include "core/svg/SVGFEColorMatrixElement.h"

#include "core/svg/graphics/filters/SVGFilterBuilder.h"
#include "core/svg_names.h"

namespace blink {

template <>
const SVGEnumerationStringEntries& GetStaticStringEntries<ColorMatrixType>() {
  DEFINE_STATIC_LOCAL(SVGEnumerationStringEntries, entries, ());
  if (entries.IsEmpty()) {
    entries.push_back(std::make_pair(FECOLORMATRIX_TYPE_MATRIX, "matrix"));
    entries.push_back(std::make_pair(FECOLORMATRIX_TYPE_SATURATE, "saturate"));
    entries.push_back(
        std::make_pair(FECOLORMATRIX_TYPE_HUEROTATE, "hueRotate"));
    entries.push_back(std::make_pair(FECOLORMATRIX_TYPE_LUMINANCETOALPHA,
                                     "luminanceToAlpha"));
  }
  return entries;
}

inline SVGFEColorMatrixElement::SVGFEColorMatrixElement(Document& document)
    : SVGFilterPrimitiveStandardAttributes(SVGNames::feColorMatrixTag,
                                           document),
      values_(SVGAnimatedNumberList::Create(this, SVGNames::valuesAttr)),
      in1_(SVGAnimatedString::Create(this, SVGNames::inAttr)),
      type_(SVGAnimatedEnumeration<ColorMatrixType>::Create(
          this,
          SVGNames::typeAttr,
          FECOLORMATRIX_TYPE_MATRIX)) {
  AddToPropertyMap(values_);
  AddToPropertyMap(in1_);
  AddToPropertyMap(type_);
}

void SVGFEColorMatrixElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(values_);
  visitor->Trace(in1_);
  visitor->Trace(type_);
  SVGFilterPrimitiveStandardAttributes::Trace(visitor);
}

DEFINE_NODE_FACTORY(SVGFEColorMatrixElement)

bool SVGFEColorMatrixElement::SetFilterEffectAttribute(
    FilterEffect* effect,
    const QualifiedName& attr_name) {
  FEColorMatrix* color_matrix = static_cast<FEColorMatrix*>(effect);
  if (attr_name == SVGNames::typeAttr)
    return color_matrix->SetType(type_->CurrentValue()->EnumValue());
  if (attr_name == SVGNames::valuesAttr)
    return color_matrix->SetValues(values_->CurrentValue()->ToFloatVector());

  return SVGFilterPrimitiveStandardAttributes::SetFilterEffectAttribute(
      effect, attr_name);
}

void SVGFEColorMatrixElement::SvgAttributeChanged(
    const QualifiedName& attr_name) {
  if (attr_name == SVGNames::typeAttr || attr_name == SVGNames::valuesAttr) {
    SVGElement::InvalidationGuard invalidation_guard(this);
    PrimitiveAttributeChanged(attr_name);
    return;
  }

  if (attr_name == SVGNames::inAttr) {
    SVGElement::InvalidationGuard invalidation_guard(this);
    Invalidate();
    return;
  }

  SVGFilterPrimitiveStandardAttributes::SvgAttributeChanged(attr_name);
}

FilterEffect* SVGFEColorMatrixElement::Build(SVGFilterBuilder* filter_builder,
                                             Filter* filter) {
  FilterEffect* input1 = filter_builder->GetEffectById(
      AtomicString(in1_->CurrentValue()->Value()));
  DCHECK(input1);

  ColorMatrixType filter_type = type_->CurrentValue()->EnumValue();
  Vector<float> filter_values = values_->CurrentValue()->ToFloatVector();
  FilterEffect* effect =
      FEColorMatrix::Create(filter, filter_type, filter_values);
  effect->InputEffects().push_back(input1);
  return effect;
}

bool SVGFEColorMatrixElement::TaintsOrigin(bool inputs_taint_origin) const {
  return inputs_taint_origin;
}

}  // namespace blink
