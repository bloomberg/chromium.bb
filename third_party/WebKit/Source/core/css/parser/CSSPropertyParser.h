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

#include "core/css/StyleRule.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include "platform/wtf/text/StringView.h"

namespace blink {

class CSSProperty;
class CSSValue;
class StylePropertyShorthand;

// Inputs: PropertyID, isImportant bool, CSSParserTokenRange.
// Outputs: Vector of CSSProperties

class CSSPropertyParser {
  WTF_MAKE_NONCOPYABLE(CSSPropertyParser);
  STACK_ALLOCATED();

 public:
  static bool ParseValue(CSSPropertyID,
                         bool important,
                         const CSSParserTokenRange&,
                         const CSSParserContext*,
                         HeapVector<CSSProperty, 256>&,
                         StyleRule::RuleType);

  // Parses a non-shorthand CSS property
  static const CSSValue* ParseSingleValue(CSSPropertyID,
                                          const CSSParserTokenRange&,
                                          const CSSParserContext*);

 private:
  CSSPropertyParser(const CSSParserTokenRange&,
                    const CSSParserContext*,
                    HeapVector<CSSProperty, 256>*);

  // TODO(timloh): Rename once the CSSParserValue-based parseValue is removed
  bool ParseValueStart(CSSPropertyID unresolved_property, bool important);
  bool ConsumeCSSWideKeyword(CSSPropertyID unresolved_property, bool important);
  const CSSValue* ParseSingleValue(CSSPropertyID,
                                   CSSPropertyID = CSSPropertyInvalid);

  bool InQuirksMode() const { return IsQuirksModeBehavior(context_->Mode()); }

  bool ParseViewportDescriptor(CSSPropertyID prop_id, bool important);
  bool ParseFontFaceDescriptor(CSSPropertyID);

  void AddParsedProperty(CSSPropertyID resolved_property,
                         CSSPropertyID current_shorthand,
                         const CSSValue&,
                         bool important,
                         bool implicit = false);
  void AddExpandedPropertyForValue(CSSPropertyID prop_id,
                                   const CSSValue&,
                                   bool);

  bool ConsumeBorder(bool important);

  bool ParseShorthand(CSSPropertyID, bool important);
  bool Consume2Values(const StylePropertyShorthand&, bool important);

  // Legacy parsing allows <string>s for animation-name
  bool ConsumeAnimationShorthand(const StylePropertyShorthand&,
                                 bool use_legacy_parsing,
                                 bool important);
  bool ConsumeBackgroundShorthand(const StylePropertyShorthand&,
                                  bool important);

  bool ConsumeGridItemPositionShorthand(CSSPropertyID, bool important);
  bool ConsumeGridTemplateRowsAndAreasAndColumns(CSSPropertyID, bool important);
  bool ConsumeGridTemplateShorthand(CSSPropertyID, bool important);
  bool ConsumeGridShorthand(bool important);
  bool ConsumeGridAreaShorthand(bool important);

  bool ConsumePlaceContentShorthand(bool important);
  bool ConsumePlaceItemsShorthand(bool important);
  bool ConsumePlaceSelfShorthand(bool important);

  bool ConsumeLegacyBreakProperty(CSSPropertyID, bool important);

 private:
  // Inputs:
  CSSParserTokenRange range_;
  Member<const CSSParserContext> context_;
  // Outputs:
  HeapVector<CSSProperty, 256>* parsed_properties_;
};

CSSPropertyID UnresolvedCSSPropertyID(StringView);
CSSValueID CssValueKeywordID(StringView);

}  // namespace blink

#endif  // CSSPropertyParser_h
