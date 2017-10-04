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

#include "core/svg/SVGComponentTransferFunctionElement.h"

#include "core/dom/Attribute.h"
#include "core/svg/SVGFEComponentTransferElement.h"
#include "core/svg/SVGNumberList.h"
#include "core/svg_names.h"

namespace blink {

template <>
const SVGEnumerationStringEntries&
GetStaticStringEntries<ComponentTransferType>() {
  DEFINE_STATIC_LOCAL(SVGEnumerationStringEntries, entries, ());
  if (entries.IsEmpty()) {
    entries.push_back(
        std::make_pair(FECOMPONENTTRANSFER_TYPE_IDENTITY, "identity"));
    entries.push_back(std::make_pair(FECOMPONENTTRANSFER_TYPE_TABLE, "table"));
    entries.push_back(
        std::make_pair(FECOMPONENTTRANSFER_TYPE_DISCRETE, "discrete"));
    entries.push_back(
        std::make_pair(FECOMPONENTTRANSFER_TYPE_LINEAR, "linear"));
    entries.push_back(std::make_pair(FECOMPONENTTRANSFER_TYPE_GAMMA, "gamma"));
  }
  return entries;
}

SVGComponentTransferFunctionElement::SVGComponentTransferFunctionElement(
    const QualifiedName& tag_name,
    Document& document)
    : SVGElement(tag_name, document),
      table_values_(
          SVGAnimatedNumberList::Create(this, SVGNames::tableValuesAttr)),
      slope_(SVGAnimatedNumber::Create(this,
                                       SVGNames::slopeAttr,
                                       SVGNumber::Create(1))),
      intercept_(SVGAnimatedNumber::Create(this,
                                           SVGNames::interceptAttr,
                                           SVGNumber::Create())),
      amplitude_(SVGAnimatedNumber::Create(this,
                                           SVGNames::amplitudeAttr,
                                           SVGNumber::Create(1))),
      exponent_(SVGAnimatedNumber::Create(this,
                                          SVGNames::exponentAttr,
                                          SVGNumber::Create(1))),
      offset_(SVGAnimatedNumber::Create(this,
                                        SVGNames::offsetAttr,
                                        SVGNumber::Create())),
      type_(SVGAnimatedEnumeration<ComponentTransferType>::Create(
          this,
          SVGNames::typeAttr,
          FECOMPONENTTRANSFER_TYPE_IDENTITY)) {
  AddToPropertyMap(table_values_);
  AddToPropertyMap(slope_);
  AddToPropertyMap(intercept_);
  AddToPropertyMap(amplitude_);
  AddToPropertyMap(exponent_);
  AddToPropertyMap(offset_);
  AddToPropertyMap(type_);
}

DEFINE_TRACE(SVGComponentTransferFunctionElement) {
  visitor->Trace(table_values_);
  visitor->Trace(slope_);
  visitor->Trace(intercept_);
  visitor->Trace(amplitude_);
  visitor->Trace(exponent_);
  visitor->Trace(offset_);
  visitor->Trace(type_);
  SVGElement::Trace(visitor);
}

void SVGComponentTransferFunctionElement::SvgAttributeChanged(
    const QualifiedName& attr_name) {
  if (attr_name == SVGNames::typeAttr ||
      attr_name == SVGNames::tableValuesAttr ||
      attr_name == SVGNames::slopeAttr ||
      attr_name == SVGNames::interceptAttr ||
      attr_name == SVGNames::amplitudeAttr ||
      attr_name == SVGNames::exponentAttr ||
      attr_name == SVGNames::offsetAttr) {
    SVGElement::InvalidationGuard invalidation_guard(this);

    InvalidateFilterPrimitiveParent(this);
    return;
  }

  SVGElement::SvgAttributeChanged(attr_name);
}

ComponentTransferFunction
SVGComponentTransferFunctionElement::TransferFunction() const {
  ComponentTransferFunction func;
  func.type = type_->CurrentValue()->EnumValue();
  func.slope = slope_->CurrentValue()->Value();
  func.intercept = intercept_->CurrentValue()->Value();
  func.amplitude = amplitude_->CurrentValue()->Value();
  func.exponent = exponent_->CurrentValue()->Value();
  func.offset = offset_->CurrentValue()->Value();
  func.table_values = table_values_->CurrentValue()->ToFloatVector();
  return func;
}

}  // namespace blink
