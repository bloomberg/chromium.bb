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

#include "core/style/StyleMultiColData.h"

#include "core/style/ComputedStyle.h"

namespace blink {

StyleMultiColData::StyleMultiColData()
    : width_(0),
      count_(ComputedStyle::InitialColumnCount()),
      gap_(0),
      visited_link_column_rule_color_(StyleColor::CurrentColor()),
      auto_width_(true),
      auto_count_(true),
      normal_gap_(true),
      fill_(ComputedStyle::InitialColumnFill()),
      column_span_(false) {}

StyleMultiColData::StyleMultiColData(const StyleMultiColData& o)
    : RefCounted<StyleMultiColData>(),
      width_(o.width_),
      count_(o.count_),
      gap_(o.gap_),
      rule_(o.rule_),
      visited_link_column_rule_color_(o.visited_link_column_rule_color_),
      auto_width_(o.auto_width_),
      auto_count_(o.auto_count_),
      normal_gap_(o.normal_gap_),
      fill_(o.fill_),
      column_span_(o.column_span_) {}

bool StyleMultiColData::operator==(const StyleMultiColData& o) const {
  return width_ == o.width_ && count_ == o.count_ && gap_ == o.gap_ &&
         rule_ == o.rule_ &&
         visited_link_column_rule_color_ == o.visited_link_column_rule_color_ &&
         auto_width_ == o.auto_width_ && auto_count_ == o.auto_count_ &&
         normal_gap_ == o.normal_gap_ && fill_ == o.fill_ &&
         column_span_ == o.column_span_;
}

}  // namespace blink
