/*
 * Copyright (C) 2006 Apple Inc. All rights reserved.
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

#ifndef SVGForeignObjectElement_h
#define SVGForeignObjectElement_h

#include "core/svg/SVGAnimatedLength.h"
#include "core/svg/SVGGraphicsElement.h"
#include "platform/heap/Handle.h"

namespace blink {

class SVGForeignObjectElement final : public SVGGraphicsElement {
  DEFINE_WRAPPERTYPEINFO();

 public:
  DECLARE_NODE_FACTORY(SVGForeignObjectElement);

  SVGAnimatedLength* x() const { return x_.Get(); }
  SVGAnimatedLength* y() const { return y_.Get(); }
  SVGAnimatedLength* width() const { return width_.Get(); }
  SVGAnimatedLength* height() const { return height_.Get(); }

  virtual void Trace(blink::Visitor*);

 private:
  explicit SVGForeignObjectElement(Document&);

  void CollectStyleForPresentationAttribute(const QualifiedName&,
                                            const AtomicString&,
                                            MutableStylePropertySet*) override;
  void SvgAttributeChanged(const QualifiedName&) override;

  bool LayoutObjectIsNeeded(const ComputedStyle&) override;
  LayoutObject* CreateLayoutObject(const ComputedStyle&) override;

  bool SelfHasRelativeLengths() const override;

  Member<SVGAnimatedLength> x_;
  Member<SVGAnimatedLength> y_;
  Member<SVGAnimatedLength> width_;
  Member<SVGAnimatedLength> height_;
};

}  // namespace blink

#endif  // SVGForeignObjectElement_h
