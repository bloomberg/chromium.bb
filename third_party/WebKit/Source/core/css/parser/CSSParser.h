// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSParser_h
#define CSSParser_h

#include "core/CSSPropertyNames.h"
#include "core/CoreExport.h"
#include "core/css/StylePropertySet.h"
#include "core/css/parser/CSSParserContext.h"
#include <memory>

namespace blink {

class Color;
class CSSParserObserver;
class CSSParserTokenRange;
class CSSSelectorList;
class Element;
class ImmutableStylePropertySet;
class StyleRuleBase;
class StyleRuleKeyframe;
class StyleSheetContents;
class CSSValue;

// This class serves as the public API for the css/parser subsystem
class CORE_EXPORT CSSParser {
  STATIC_ONLY(CSSParser);

 public:
  // As well as regular rules, allows @import and @namespace but not @charset
  static StyleRuleBase* ParseRule(const CSSParserContext*,
                                  StyleSheetContents*,
                                  const String&);
  static void ParseSheet(const CSSParserContext*,
                         StyleSheetContents*,
                         const String&,
                         bool defer_property_parsing = false);
  static CSSSelectorList ParseSelector(const CSSParserContext*,
                                       StyleSheetContents*,
                                       const String&);
  static CSSSelectorList ParsePageSelector(const CSSParserContext*,
                                           StyleSheetContents*,
                                           const String&);
  static bool ParseDeclarationList(const CSSParserContext*,
                                   MutableStylePropertySet*,
                                   const String&);

  static MutableStylePropertySet::SetResult ParseValue(
      MutableStylePropertySet*,
      CSSPropertyID unresolved_property,
      const String&,
      bool important);
  static MutableStylePropertySet::SetResult ParseValue(
      MutableStylePropertySet*,
      CSSPropertyID unresolved_property,
      const String&,
      bool important,
      StyleSheetContents*);

  static MutableStylePropertySet::SetResult ParseValueForCustomProperty(
      MutableStylePropertySet*,
      const AtomicString& property_name,
      const PropertyRegistry*,
      const String& value,
      bool important,
      StyleSheetContents*,
      bool is_animation_tainted);
  static ImmutableStylePropertySet* ParseCustomPropertySet(CSSParserTokenRange);

  // This is for non-shorthands only
  static const CSSValue* ParseSingleValue(
      CSSPropertyID,
      const String&,
      const CSSParserContext* = StrictCSSParserContext());

  static const CSSValue* ParseFontFaceDescriptor(CSSPropertyID,
                                                 const String&,
                                                 const CSSParserContext*);

  static ImmutableStylePropertySet* ParseInlineStyleDeclaration(const String&,
                                                                Element*);

  static std::unique_ptr<Vector<double>> ParseKeyframeKeyList(const String&);
  static StyleRuleKeyframe* ParseKeyframeRule(const CSSParserContext*,
                                              const String&);

  static bool ParseSupportsCondition(const String&);

  // The color will only be changed when string contains a valid CSS color, so
  // callers can set it to a default color and ignore the boolean result.
  static bool ParseColor(Color&, const String&, bool strict = false);
  static bool ParseSystemColor(Color&, const String&);

  static void ParseSheetForInspector(const CSSParserContext*,
                                     StyleSheetContents*,
                                     const String&,
                                     CSSParserObserver&);
  static void ParseDeclarationListForInspector(const CSSParserContext*,
                                               const String&,
                                               CSSParserObserver&);

 private:
  static MutableStylePropertySet::SetResult ParseValue(
      MutableStylePropertySet*,
      CSSPropertyID unresolved_property,
      const String&,
      bool important,
      const CSSParserContext*);
};

}  // namespace blink

#endif  // CSSParser_h
