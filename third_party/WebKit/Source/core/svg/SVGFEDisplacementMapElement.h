/*
 * Copyright (C) 2006 Oliver Hunt <oliver@nerget.com>
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

#ifndef SVGFEDisplacementMapElement_h
#define SVGFEDisplacementMapElement_h

#include "core/svg/SVGAnimatedEnumeration.h"
#include "core/svg/SVGAnimatedNumber.h"
#include "core/svg/SVGFilterPrimitiveStandardAttributes.h"
#include "platform/graphics/filters/FEDisplacementMap.h"
#include "platform/heap/Handle.h"

namespace blink {

template <>
const SVGEnumerationStringEntries&
GetStaticStringEntries<ChannelSelectorType>();

class SVGFEDisplacementMapElement final
    : public SVGFilterPrimitiveStandardAttributes {
  DEFINE_WRAPPERTYPEINFO();

 public:
  DECLARE_NODE_FACTORY(SVGFEDisplacementMapElement);

  static ChannelSelectorType StringToChannel(const String&);

  SVGAnimatedNumber* scale() { return scale_.Get(); }
  SVGAnimatedString* in1() { return in1_.Get(); }
  SVGAnimatedString* in2() { return in2_.Get(); }
  SVGAnimatedEnumeration<ChannelSelectorType>* xChannelSelector() {
    return x_channel_selector_.Get();
  }
  SVGAnimatedEnumeration<ChannelSelectorType>* yChannelSelector() {
    return y_channel_selector_.Get();
  }

  virtual void Trace(blink::Visitor*);

 private:
  explicit SVGFEDisplacementMapElement(Document&);

  bool SetFilterEffectAttribute(FilterEffect*,
                                const QualifiedName& attr_name) override;
  void SvgAttributeChanged(const QualifiedName&) override;
  FilterEffect* Build(SVGFilterBuilder*, Filter*) override;

  Member<SVGAnimatedNumber> scale_;
  Member<SVGAnimatedString> in1_;
  Member<SVGAnimatedString> in2_;
  Member<SVGAnimatedEnumeration<ChannelSelectorType>> x_channel_selector_;
  Member<SVGAnimatedEnumeration<ChannelSelectorType>> y_channel_selector_;
};

}  // namespace blink

#endif  // SVGFEDisplacementMapElement_h
