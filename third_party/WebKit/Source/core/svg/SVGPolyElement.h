/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
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

#ifndef SVGPolyElement_h
#define SVGPolyElement_h

#include "core/svg/SVGAnimatedPointList.h"
#include "core/svg/SVGGeometryElement.h"
#include "core/svg_names.h"
#include "platform/heap/Handle.h"

namespace blink {

class SVGPolyElement : public SVGGeometryElement {
 public:
  SVGAnimatedPointList* Points() const { return points_.Get(); }

  SVGPointListTearOff* pointsFromJavascript() { return points_->baseVal(); }
  SVGPointListTearOff* animatedPoints() { return points_->animVal(); }

  virtual void Trace(blink::Visitor*);

 protected:
  SVGPolyElement(const QualifiedName&, Document&);

  Path AsPathFromPoints() const;

 private:
  void SvgAttributeChanged(const QualifiedName&) final;

 private:
  Member<SVGAnimatedPointList> points_;
};

inline bool IsSVGPolyElement(const SVGElement& element) {
  return element.HasTagName(SVGNames::polygonTag) ||
         element.HasTagName(SVGNames::polylineTag);
}

DEFINE_SVGELEMENT_TYPE_CASTS_WITH_FUNCTION(SVGPolyElement);

}  // namespace blink

#endif
