/*
 * Copyright (C) 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2008 Apple Inc. All right reserved.
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

#ifndef CSSCursorImageValue_h
#define CSSCursorImageValue_h

#include "core/css/CSSValue.h"
#include "platform/geometry/IntPoint.h"

namespace blink {

class CSSCursorImageValue : public CSSValue {
 public:
  static const CSSCursorImageValue* create(const CSSValue& imageValue,
                                           bool hotSpotSpecified,
                                           const IntPoint& hotSpot) {
    return new CSSCursorImageValue(imageValue, hotSpotSpecified, hotSpot);
  }

  ~CSSCursorImageValue();

  bool hotSpotSpecified() const { return m_hotSpotSpecified; }
  const IntPoint& hotSpot() const { return m_hotSpot; }
  const CSSValue& imageValue() const { return *m_imageValue; }

  String customCSSText() const;

  bool equals(const CSSCursorImageValue&) const;

  DECLARE_TRACE_AFTER_DISPATCH();

 private:
  CSSCursorImageValue(const CSSValue& imageValue,
                      bool hotSpotSpecified,
                      const IntPoint& hotSpot);

  Member<const CSSValue> m_imageValue;
  IntPoint m_hotSpot;
  bool m_hotSpotSpecified;
};

DEFINE_CSS_VALUE_TYPE_CASTS(CSSCursorImageValue, isCursorImageValue());

}  // namespace blink

#endif  // CSSCursorImageValue_h
