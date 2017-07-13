/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2005, 2006, 2007, 2012 Apple Inc. All rights reserved.
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

#include "core/css/CSSRule.h"

#include "core/css/CSSStyleSheet.h"
#include "core/css/StyleRule.h"
#include "core/css/StyleSheetContents.h"
#include "platform/bindings/ScriptWrappableVisitor.h"

namespace blink {

struct SameSizeAsCSSRule : public GarbageCollectedFinalized<SameSizeAsCSSRule>,
                           public ScriptWrappable {
  virtual ~SameSizeAsCSSRule();
  unsigned char bitfields;
  void* pointer_union;
};

static_assert(sizeof(CSSRule) == sizeof(SameSizeAsCSSRule),
              "CSSRule should stay small");

const CSSParserContext* CSSRule::ParserContext() const {
  CSSStyleSheet* style_sheet = parentStyleSheet();
  return style_sheet ? style_sheet->Contents()->ParserContext()
                     : StrictCSSParserContext();
}

void CSSRule::SetParentStyleSheet(CSSStyleSheet* style_sheet) {
  parent_is_rule_ = false;
  parent_style_sheet_ = style_sheet;
  ScriptWrappableVisitor::WriteBarrier(this, parent_style_sheet_);
}

void CSSRule::SetParentRule(CSSRule* rule) {
  parent_is_rule_ = true;
  parent_rule_ = rule;
  ScriptWrappableVisitor::WriteBarrier(this, parent_rule_);
}

DEFINE_TRACE(CSSRule) {
  // This makes the parent link strong, which is different from the
  // pre-oilpan world, where the parent link is mysteriously zeroed under
  // some circumstances.
  if (parent_is_rule_)
    visitor->Trace(parent_rule_);
  else
    visitor->Trace(parent_style_sheet_);
}

DEFINE_TRACE_WRAPPERS(CSSRule) {
  if (parent_is_rule_)
    visitor->TraceWrappersWithManualWriteBarrier(parent_rule_);
  else
    visitor->TraceWrappersWithManualWriteBarrier(parent_style_sheet_);
}

}  // namespace blink
