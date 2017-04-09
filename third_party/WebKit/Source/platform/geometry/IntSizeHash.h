/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
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
#ifndef IntSizeHash_h
#define IntSizeHash_h

#include "platform/geometry/IntSize.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/HashSet.h"

namespace WTF {

template <>
struct DefaultHash<blink::IntSize> {
  STATIC_ONLY(DefaultHash);
  struct Hash {
    STATIC_ONLY(Hash);
    static unsigned GetHash(const blink::IntSize& key) {
      return HashInts(key.Width(), key.Height());
    }
    static bool Equal(const blink::IntSize& a, const blink::IntSize& b) {
      return a == b;
    }
    static const bool safe_to_compare_to_empty_or_deleted = true;
  };
};

template <>
struct HashTraits<blink::IntSize> : GenericHashTraits<blink::IntSize> {
  STATIC_ONLY(HashTraits);
  static const bool kEmptyValueIsZero = true;
  static void ConstructDeletedValue(blink::IntSize& slot, bool) {
    new (NotNull, &slot) blink::IntSize(-1, -1);
  }
  static bool IsDeletedValue(const blink::IntSize& value) {
    return value.Width() == -1 && value.Height() == -1;
  }
};

}  // namespace WTF

#endif
