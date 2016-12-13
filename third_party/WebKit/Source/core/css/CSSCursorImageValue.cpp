/*
 * Copyright (C) 2006 Rob Buis <buis@kde.org>
 *           (C) 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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

#include "core/css/CSSCursorImageValue.h"

#include "wtf/text/StringBuilder.h"
#include "wtf/text/WTFString.h"

namespace blink {

CSSCursorImageValue::CSSCursorImageValue(const CSSValue& imageValue,
                                         bool hotSpotSpecified,
                                         const IntPoint& hotSpot)
    : CSSValue(CursorImageClass),
      m_imageValue(&imageValue),
      m_hotSpot(hotSpot),
      m_hotSpotSpecified(hotSpotSpecified) {
  DCHECK(imageValue.isImageValue() || imageValue.isImageSetValue());
}

CSSCursorImageValue::~CSSCursorImageValue() {}

String CSSCursorImageValue::customCSSText() const {
  StringBuilder result;
  result.append(m_imageValue->cssText());
  if (m_hotSpotSpecified) {
    result.append(' ');
    result.appendNumber(m_hotSpot.x());
    result.append(' ');
    result.appendNumber(m_hotSpot.y());
  }
  return result.toString();
}

bool CSSCursorImageValue::equals(const CSSCursorImageValue& other) const {
  return (m_hotSpotSpecified
              ? other.m_hotSpotSpecified && m_hotSpot == other.m_hotSpot
              : !other.m_hotSpotSpecified) &&
         compareCSSValuePtr(m_imageValue, other.m_imageValue);
}

DEFINE_TRACE_AFTER_DISPATCH(CSSCursorImageValue) {
  visitor->trace(m_imageValue);
  CSSValue::traceAfterDispatch(visitor);
}

}  // namespace blink
