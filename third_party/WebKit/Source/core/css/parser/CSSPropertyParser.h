/*
 * Copyright (C) 2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 - 2010  Torch Mobile (Beijing) Co. Ltd. All rights
 * reserved.
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

#ifndef CSSPropertyParser_h
#define CSSPropertyParser_h

#include "base/macros.h"
#include "core/css/StyleRule.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include "platform/wtf/text/StringView.h"

namespace blink {

class CSSPropertyValue;
class CSSValue;

// Inputs: PropertyID, isImportant bool, CSSParserTokenRange.
// Outputs: Vector of CSSProperties

class CSSPropertyParser {
  STACK_ALLOCATED();

 public:
  static bool ParseValue(CSSPropertyID,
                         bool important,
                         const CSSParserTokenRange&,
                         const CSSParserContext*,
                         HeapVector<CSSPropertyValue, 256>&,
                         StyleRule::RuleType);

  // Parses a non-shorthand CSS property
  static const CSSValue* ParseSingleValue(CSSPropertyID,
                                          const CSSParserTokenRange&,
                                          const CSSParserContext*);

 private:
  CSSPropertyParser(const CSSParserTokenRange&,
                    const CSSParserContext*,
                    HeapVector<CSSPropertyValue, 256>*);

  // TODO(timloh): Rename once the CSSParserValue-based parseValue is removed
  bool ParseValueStart(CSSPropertyID unresolved_property, bool important);
  bool ConsumeCSSWideKeyword(CSSPropertyID unresolved_property, bool important);

  bool ParseViewportDescriptor(CSSPropertyID prop_id, bool important);
  bool ParseFontFaceDescriptor(CSSPropertyID);

 private:
  // Inputs:
  CSSParserTokenRange range_;
  Member<const CSSParserContext> context_;
  // Outputs:
  HeapVector<CSSPropertyValue, 256>* parsed_properties_;
  DISALLOW_COPY_AND_ASSIGN(CSSPropertyParser);
};

CSSPropertyID UnresolvedCSSPropertyID(StringView);
CSSValueID CssValueKeywordID(StringView);

}  // namespace blink

#endif  // CSSPropertyParser_h
