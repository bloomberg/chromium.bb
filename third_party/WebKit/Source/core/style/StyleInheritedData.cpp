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
    : horizontal_border_spacing(
          ComputedStyle::InitialHorizontalBorderSpacing()),
      vertical_border_spacing(ComputedStyle::InitialVerticalBorderSpacing()),
      line_height(ComputedStyle::InitialLineHeight()),
      color(ComputedStyle::InitialColor()),
      visited_link_color(ComputedStyle::InitialColor()),
      text_autosizing_multiplier(1) {}

StyleInheritedData::~StyleInheritedData() {}

StyleInheritedData::StyleInheritedData(const StyleInheritedData& o)
    : RefCounted<StyleInheritedData>(),
      horizontal_border_spacing(o.horizontal_border_spacing),
      vertical_border_spacing(o.vertical_border_spacing),
      line_height(o.line_height),
      font(o.font),
      color(o.color),
      visited_link_color(o.visited_link_color),
      text_autosizing_multiplier(o.text_autosizing_multiplier) {}

bool StyleInheritedData::operator==(const StyleInheritedData& o) const {
  return line_height == o.line_height && font == o.font && color == o.color &&
         visited_link_color == o.visited_link_color &&
         horizontal_border_spacing == o.horizontal_border_spacing &&
         text_autosizing_multiplier == o.text_autosizing_multiplier &&
         vertical_border_spacing == o.vertical_border_spacing;
}

}  // namespace blink
