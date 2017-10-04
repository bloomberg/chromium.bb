/*
 * Copyright (C) 2005 Alexander Kellett <lypanov@kde.org>
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

#ifndef SVGMaskElement_h
#define SVGMaskElement_h

#include "core/svg/SVGAnimatedEnumeration.h"
#include "core/svg/SVGAnimatedLength.h"
#include "core/svg/SVGElement.h"
#include "core/svg/SVGTests.h"
#include "core/svg/SVGUnitTypes.h"
#include "platform/heap/Handle.h"

namespace blink {

class SVGMaskElement final : public SVGElement, public SVGTests {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(SVGMaskElement);

 public:
  DECLARE_NODE_FACTORY(SVGMaskElement);

  SVGAnimatedLength* x() const { return x_.Get(); }
  SVGAnimatedLength* y() const { return y_.Get(); }
  SVGAnimatedLength* width() const { return width_.Get(); }
  SVGAnimatedLength* height() const { return height_.Get(); }
  SVGAnimatedEnumeration<SVGUnitTypes::SVGUnitType>* maskUnits() {
    return mask_units_.Get();
  }
  SVGAnimatedEnumeration<SVGUnitTypes::SVGUnitType>* maskContentUnits() {
    return mask_content_units_.Get();
  }

  DECLARE_VIRTUAL_TRACE();

 private:
  explicit SVGMaskElement(Document&);

  bool IsValid() const override { return SVGTests::IsValid(); }
  bool NeedsPendingResourceHandling() const override { return false; }

  void CollectStyleForPresentationAttribute(const QualifiedName&,
                                            const AtomicString&,
                                            MutableStylePropertySet*) override;
  void SvgAttributeChanged(const QualifiedName&) override;
  void ChildrenChanged(const ChildrenChange&) override;

  LayoutObject* CreateLayoutObject(const ComputedStyle&) override;

  bool SelfHasRelativeLengths() const override;

  Member<SVGAnimatedLength> x_;
  Member<SVGAnimatedLength> y_;
  Member<SVGAnimatedLength> width_;
  Member<SVGAnimatedLength> height_;
  Member<SVGAnimatedEnumeration<SVGUnitTypes::SVGUnitType>> mask_units_;
  Member<SVGAnimatedEnumeration<SVGUnitTypes::SVGUnitType>> mask_content_units_;
};

}  // namespace blink

#endif  // SVGMaskElement_h
