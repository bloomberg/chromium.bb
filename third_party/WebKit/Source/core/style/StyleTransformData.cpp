/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
 *
 */

#include "core/style/StyleTransformData.h"

#include "core/style/ComputedStyle.h"
#include "core/style/DataEquivalency.h"

namespace blink {

StyleTransformData::StyleTransformData()
    : operations_(ComputedStyle::InitialTransform()),
      origin_(ComputedStyle::InitialTransformOrigin()),
      motion_(ComputedStyle::InitialOffsetAnchor(),
              ComputedStyle::InitialOffsetPosition(),
              nullptr,
              ComputedStyle::InitialOffsetDistance(),
              ComputedStyle::InitialOffsetRotate()),
      translate_(ComputedStyle::InitialTranslate()),
      rotate_(ComputedStyle::InitialRotate()),
      scale_(ComputedStyle::InitialScale()) {}

StyleTransformData::StyleTransformData(const StyleTransformData& o)
    : RefCounted<StyleTransformData>(),
      operations_(o.operations_),
      origin_(o.origin_),
      motion_(o.motion_),
      translate_(o.translate_),
      rotate_(o.rotate_),
      scale_(o.scale_) {}

bool StyleTransformData::operator==(const StyleTransformData& o) const {
  return origin_ == o.origin_ && operations_ == o.operations_ &&
         motion_ == o.motion_ && DataEquivalent(translate_, o.translate_) &&
         DataEquivalent(rotate_, o.rotate_) && DataEquivalent(scale_, o.scale_);
}

}  // namespace blink
