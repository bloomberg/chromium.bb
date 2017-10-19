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

namespace cssvalue {

class CSSCursorImageValue : public CSSValue {
 public:
  static const CSSCursorImageValue* Create(const CSSValue& image_value,
                                           bool hot_spot_specified,
                                           const IntPoint& hot_spot) {
    return new CSSCursorImageValue(image_value, hot_spot_specified, hot_spot);
  }

  ~CSSCursorImageValue();

  bool HotSpotSpecified() const { return hot_spot_specified_; }
  const IntPoint& HotSpot() const { return hot_spot_; }
  const CSSValue& ImageValue() const { return *image_value_; }

  String CustomCSSText() const;

  bool Equals(const CSSCursorImageValue&) const;

  void TraceAfterDispatch(blink::Visitor*);

 private:
  CSSCursorImageValue(const CSSValue& image_value,
                      bool hot_spot_specified,
                      const IntPoint& hot_spot);

  Member<const CSSValue> image_value_;
  IntPoint hot_spot_;
  bool hot_spot_specified_;
};

DEFINE_CSS_VALUE_TYPE_CASTS(CSSCursorImageValue, IsCursorImageValue());

}  // namespace cssvalue

}  // namespace blink

#endif  // CSSCursorImageValue_h
