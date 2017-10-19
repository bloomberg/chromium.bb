/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#ifndef SVGFilterElement_h
#define SVGFilterElement_h

#include "core/CoreExport.h"
#include "core/svg/SVGAnimatedEnumeration.h"
#include "core/svg/SVGAnimatedLength.h"
#include "core/svg/SVGElement.h"
#include "core/svg/SVGURIReference.h"
#include "core/svg/SVGUnitTypes.h"
#include "platform/heap/Handle.h"

namespace blink {

class SVGFilterPrimitiveStandardAttributes;

class CORE_EXPORT SVGFilterElement final : public SVGElement,
                                           public SVGURIReference {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(SVGFilterElement);

 public:
  DECLARE_NODE_FACTORY(SVGFilterElement);
  virtual void Trace(blink::Visitor*);

  ~SVGFilterElement() override;

  SVGAnimatedLength* x() const { return x_.Get(); }
  SVGAnimatedLength* y() const { return y_.Get(); }
  SVGAnimatedLength* width() const { return width_.Get(); }
  SVGAnimatedLength* height() const { return height_.Get(); }
  SVGAnimatedEnumeration<SVGUnitTypes::SVGUnitType>* filterUnits() {
    return filter_units_.Get();
  }
  SVGAnimatedEnumeration<SVGUnitTypes::SVGUnitType>* primitiveUnits() {
    return primitive_units_.Get();
  }

  // Fine-grained invalidation of a specific property on a specific primitive.
  void PrimitiveAttributeChanged(SVGFilterPrimitiveStandardAttributes&,
                                 const QualifiedName&);

  // Invalidate the entire filter chain.
  void InvalidateFilterChain();

 private:
  explicit SVGFilterElement(Document&);

  bool NeedsPendingResourceHandling() const override { return false; }

  void SvgAttributeChanged(const QualifiedName&) override;
  void ChildrenChanged(const ChildrenChange&) override;

  LayoutObject* CreateLayoutObject(const ComputedStyle&) override;

  bool SelfHasRelativeLengths() const override;

  Member<SVGAnimatedLength> x_;
  Member<SVGAnimatedLength> y_;
  Member<SVGAnimatedLength> width_;
  Member<SVGAnimatedLength> height_;
  Member<SVGAnimatedEnumeration<SVGUnitTypes::SVGUnitType>> filter_units_;
  Member<SVGAnimatedEnumeration<SVGUnitTypes::SVGUnitType>> primitive_units_;
};

}  // namespace blink

#endif  // SVGFilterElement_h
