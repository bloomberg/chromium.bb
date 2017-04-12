/*
 * Copyright (C) Research In Motion Limited 2010-2011. All rights reserved.
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

#ifndef SVGCharacterData_h
#define SVGCharacterData_h

#include "platform/wtf/Allocator.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/MathExtras.h"

namespace blink {

struct SVGCharacterData {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
  SVGCharacterData();

  static float EmptyValue() { return std::numeric_limits<float>::quiet_NaN(); }
  static bool IsEmptyValue(float value) { return std::isnan(value); }

  bool HasX() const { return !IsEmptyValue(x); }
  bool HasY() const { return !IsEmptyValue(y); }
  bool HasDx() const { return !IsEmptyValue(dx); }
  bool HasDy() const { return !IsEmptyValue(dy); }
  bool HasRotate() const { return !IsEmptyValue(rotate); }

  float x;
  float y;
  float dx;
  float dy;
  float rotate;
};

inline SVGCharacterData::SVGCharacterData()
    : x(EmptyValue()),
      y(EmptyValue()),
      dx(EmptyValue()),
      dy(EmptyValue()),
      rotate(EmptyValue()) {}

typedef HashMap<unsigned, SVGCharacterData> SVGCharacterDataMap;

}  // namespace blink

#endif
