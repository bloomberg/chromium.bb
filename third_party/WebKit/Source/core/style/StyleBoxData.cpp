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

#include "core/style/StyleBoxData.h"

#include "core/style/ComputedStyle.h"

namespace blink {

struct SameSizeAsStyleBoxData : public RefCounted<SameSizeAsStyleBoxData> {
  Length length[7];
  int z_index_;
  uint32_t bitfields;
};

static_assert(sizeof(StyleBoxData) == sizeof(SameSizeAsStyleBoxData),
              "StyleBoxData should stay small");

StyleBoxData::StyleBoxData()
    : min_width_(ComputedStyle::InitialMinSize()),
      max_width_(ComputedStyle::InitialMaxSize()),
      min_height_(ComputedStyle::InitialMinSize()),
      max_height_(ComputedStyle::InitialMaxSize()),
      z_index_(0),
      has_auto_z_index_(true),
      box_sizing_(static_cast<unsigned>(ComputedStyle::InitialBoxSizing())),
      box_decoration_break_(kBoxDecorationBreakSlice) {}

bool StyleBoxData::operator==(const StyleBoxData& o) const {
  return width_ == o.width_ && height_ == o.height_ &&
         min_width_ == o.min_width_ && max_width_ == o.max_width_ &&
         min_height_ == o.min_height_ && max_height_ == o.max_height_ &&
         vertical_align_ == o.vertical_align_ && z_index_ == o.z_index_ &&
         has_auto_z_index_ == o.has_auto_z_index_ &&
         box_sizing_ == o.box_sizing_ &&
         box_decoration_break_ == o.box_decoration_break_;
}

}  // namespace blink
