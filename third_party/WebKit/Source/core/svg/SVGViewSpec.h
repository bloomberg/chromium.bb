/*
 * Copyright (C) 2007 Rob Buis <buis@kde.org>
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

#ifndef SVGViewSpec_h
#define SVGViewSpec_h

#include "core/svg/SVGZoomAndPan.h"
#include "platform/heap/Handle.h"

namespace blink {

class FloatRect;
class SVGPreserveAspectRatio;
class SVGRect;
class SVGTransformList;

class SVGViewSpec final : public GarbageCollectedFinalized<SVGViewSpec>,
                          public SVGZoomAndPan {
 public:
  static SVGViewSpec* create() { return new SVGViewSpec(); }

  bool parseViewSpec(const String&);
  void reset();
  template <typename T>
  void inheritViewAttributesFromElement(T*);

  SVGRect* viewBox() { return m_viewBox; }
  SVGPreserveAspectRatio* preserveAspectRatio() {
    return m_preserveAspectRatio;
  }
  SVGTransformList* transform() { return m_transform; }

  DECLARE_VIRTUAL_TRACE();

 private:
  SVGViewSpec();

  template <typename CharType>
  bool parseViewSpecInternal(const CharType* ptr, const CharType* end);

  void setViewBox(const FloatRect&);
  void setPreserveAspectRatio(const SVGPreserveAspectRatio&);

  Member<SVGRect> m_viewBox;
  Member<SVGPreserveAspectRatio> m_preserveAspectRatio;
  Member<SVGTransformList> m_transform;
};

template <typename T>
void SVGViewSpec::inheritViewAttributesFromElement(T* inheritFromElement) {
  if (!inheritFromElement->hasEmptyViewBox())
    setViewBox(inheritFromElement->viewBox()->currentValue()->value());

  if (inheritFromElement->preserveAspectRatio()->isSpecified()) {
    setPreserveAspectRatio(
        *inheritFromElement->preserveAspectRatio()->currentValue());
  }

  if (inheritFromElement->hasAttribute(SVGNames::zoomAndPanAttr))
    setZoomAndPan(inheritFromElement->zoomAndPan());
}

}  // namespace blink

#endif  // SVGViewSpec_h
