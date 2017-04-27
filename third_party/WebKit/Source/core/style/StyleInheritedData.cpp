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

#include "core/style/StyleInheritedData.h"

#include "core/style/ComputedStyle.h"

namespace blink {

StyleInheritedData::StyleInheritedData()
    : horizontal_border_spacing_(
          ComputedStyle::InitialHorizontalBorderSpacing()),
      vertical_border_spacing_(ComputedStyle::InitialVerticalBorderSpacing()),
      line_height_(ComputedStyle::InitialLineHeight()),
      color_(ComputedStyle::InitialColor()),
      visited_link_color_(ComputedStyle::InitialColor()),
      text_autosizing_multiplier_(1) {}

StyleInheritedData::~StyleInheritedData() {}

StyleInheritedData::StyleInheritedData(const StyleInheritedData& o)
    : RefCounted<StyleInheritedData>(),
      horizontal_border_spacing_(o.horizontal_border_spacing_),
      vertical_border_spacing_(o.vertical_border_spacing_),
      line_height_(o.line_height_),
      font_(o.font_),
      color_(o.color_),
      visited_link_color_(o.visited_link_color_),
      text_autosizing_multiplier_(o.text_autosizing_multiplier_) {}

bool StyleInheritedData::operator==(const StyleInheritedData& o) const {
  return line_height_ == o.line_height_ && font_ == o.font_ &&
         color_ == o.color_ && visited_link_color_ == o.visited_link_color_ &&
         horizontal_border_spacing_ == o.horizontal_border_spacing_ &&
         text_autosizing_multiplier_ == o.text_autosizing_multiplier_ &&
         vertical_border_spacing_ == o.vertical_border_spacing_;
}

}  // namespace blink
