/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2008 Rob Buis <buis@kde.org>
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

#ifndef SVGTextPositioningElement_h
#define SVGTextPositioningElement_h

#include "core/svg/SVGAnimatedLengthList.h"
#include "core/svg/SVGAnimatedNumberList.h"
#include "core/svg/SVGTextContentElement.h"
#include "platform/heap/Handle.h"

namespace blink {

class SVGTextPositioningElement : public SVGTextContentElement {
  DEFINE_WRAPPERTYPEINFO();

 public:
  SVGAnimatedLengthList* x() { return x_.Get(); }
  SVGAnimatedLengthList* y() { return y_.Get(); }
  SVGAnimatedLengthList* dx() { return dx_.Get(); }
  SVGAnimatedLengthList* dy() { return dy_.Get(); }
  SVGAnimatedNumberList* rotate() { return rotate_.Get(); }

  DECLARE_VIRTUAL_TRACE();

 protected:
  SVGTextPositioningElement(const QualifiedName&, Document&);

  void SvgAttributeChanged(const QualifiedName&) final;
  bool IsTextPositioning() const final { return true; }

  Member<SVGAnimatedLengthList> x_;
  Member<SVGAnimatedLengthList> y_;
  Member<SVGAnimatedLengthList> dx_;
  Member<SVGAnimatedLengthList> dy_;
  Member<SVGAnimatedNumberList> rotate_;
};

inline bool IsSVGTextPositioningElement(const SVGElement& element) {
  return element.IsTextPositioning();
}

DEFINE_SVGELEMENT_TYPE_CASTS_WITH_FUNCTION(SVGTextPositioningElement);

}  // namespace blink

#endif  // SVGTextPositioningElement_h
