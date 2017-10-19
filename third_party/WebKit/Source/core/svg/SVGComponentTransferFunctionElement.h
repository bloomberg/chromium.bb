/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
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

#ifndef SVGComponentTransferFunctionElement_h
#define SVGComponentTransferFunctionElement_h

#include "core/svg/SVGAnimatedEnumeration.h"
#include "core/svg/SVGAnimatedNumber.h"
#include "core/svg/SVGAnimatedNumberList.h"
#include "core/svg/SVGElement.h"
#include "platform/graphics/filters/FEComponentTransfer.h"
#include "platform/heap/Handle.h"

namespace blink {

template <>
const SVGEnumerationStringEntries&
GetStaticStringEntries<ComponentTransferType>();

class SVGComponentTransferFunctionElement : public SVGElement {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ComponentTransferFunction TransferFunction() const;

  SVGAnimatedNumberList* tableValues() { return table_values_.Get(); }
  SVGAnimatedNumber* slope() { return slope_.Get(); }
  SVGAnimatedNumber* intercept() { return intercept_.Get(); }
  SVGAnimatedNumber* amplitude() { return amplitude_.Get(); }
  SVGAnimatedNumber* exponent() { return exponent_.Get(); }
  SVGAnimatedNumber* offset() { return offset_.Get(); }
  SVGAnimatedEnumeration<ComponentTransferType>* type() { return type_.Get(); }

  virtual void Trace(blink::Visitor*);

 protected:
  SVGComponentTransferFunctionElement(const QualifiedName&, Document&);

  void SvgAttributeChanged(const QualifiedName&) final;

  bool LayoutObjectIsNeeded(const ComputedStyle&) final { return false; }

 private:
  Member<SVGAnimatedNumberList> table_values_;
  Member<SVGAnimatedNumber> slope_;
  Member<SVGAnimatedNumber> intercept_;
  Member<SVGAnimatedNumber> amplitude_;
  Member<SVGAnimatedNumber> exponent_;
  Member<SVGAnimatedNumber> offset_;
  Member<SVGAnimatedEnumeration<ComponentTransferType>> type_;
};

}  // namespace blink

#endif  // SVGComponentTransferFunctionElement_h
