/*
 * Copyright (C) 2013 Google, Inc.
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc.
 * All rights reserved.
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

#ifndef StyleAdjuster_h
#define StyleAdjuster_h

#include "platform/wtf/Allocator.h"

namespace blink {

class Element;
class ComputedStyle;

// Certain CSS Properties/Values do not apply to certain elements
// and the web expects that we expose "adjusted" values when
// for those property/element pairs.
class StyleAdjuster {
  STATIC_ONLY(StyleAdjuster);

 public:
  static void AdjustComputedStyle(ComputedStyle& style_to_adjust,
                                  const ComputedStyle& parent_style,
                                  const ComputedStyle& layout_parent_style,
                                  Element*);
  static void AdjustStyleForEditing(ComputedStyle&);
  static void AdjustStyleForAlignment(ComputedStyle&,
                                      const ComputedStyle& parent_style);
};

}  // namespace blink

#endif  // StyleAdjuster_h
