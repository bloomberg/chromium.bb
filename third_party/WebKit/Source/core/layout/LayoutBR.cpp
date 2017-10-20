/**
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2006 Apple Computer, Inc.
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

#include "core/layout/LayoutBR.h"

#include "core/css/StyleEngine.h"
#include "core/dom/Document.h"
#include "core/editing/PositionWithAffinity.h"
#include "core/layout/LayoutObjectInlines.h"

namespace blink {

static scoped_refptr<StringImpl> NewlineString() {
  DEFINE_STATIC_LOCAL(const String, string, ("\n"));
  return string.Impl();
}

LayoutBR::LayoutBR(Node* node) : LayoutText(node, NewlineString()) {}

LayoutBR::~LayoutBR() {}

int LayoutBR::LineHeight(bool first_line) const {
  const ComputedStyle& style = StyleRef(
      first_line && GetDocument().GetStyleEngine().UsesFirstLineRules());
  return style.ComputedLineHeight();
}

void LayoutBR::StyleDidChange(StyleDifference diff,
                              const ComputedStyle* old_style) {
  LayoutText::StyleDidChange(diff, old_style);
}

int LayoutBR::CaretMinOffset() const {
  return 0;
}

int LayoutBR::CaretMaxOffset() const {
  return 1;
}

PositionWithAffinity LayoutBR::PositionForPoint(const LayoutPoint&) {
  return CreatePositionWithAffinity(0);
}

}  // namespace blink
