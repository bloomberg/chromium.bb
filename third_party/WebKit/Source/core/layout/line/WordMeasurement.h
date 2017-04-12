/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc.
 *               All right reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2013 Adobe Systems Incorporated.
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

#ifndef WordMeasurement_h
#define WordMeasurement_h

#include "platform/fonts/SimpleFontData.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/HashSet.h"

namespace blink {

class WordMeasurement {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  WordMeasurement()
      : layout_text(nullptr), width(0), start_offset(0), end_offset(0) {}

  LineLayoutText layout_text;
  float width;
  int start_offset;
  int end_offset;
  HashSet<const SimpleFontData*> fallback_fonts;
  FloatRect glyph_bounds;
};

}  // namespace blink

#endif  // WordMeasurement_h
