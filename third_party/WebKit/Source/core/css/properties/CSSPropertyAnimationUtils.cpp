// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAnimationUtils.h"

#include "core/StylePropertyShorthand.h"
#include "core/css/CSSInitialValue.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

bool CSSPropertyAnimationUtils::ConsumeAnimationShorthand(
    const StylePropertyShorthand& shorthand,
    HeapVector<Member<CSSValueList>, kMaxNumAnimationLonghands>& longhands,
    ConsumeAnimationItemValue consumeLonghandItem,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    bool use_legacy_parsing) {
  DCHECK(consumeLonghandItem);
  const unsigned longhand_count = shorthand.length();
  DCHECK_LE(longhand_count, kMaxNumAnimationLonghands);

  for (size_t i = 0; i < longhand_count; ++i)
    longhands[i] = CSSValueList::CreateCommaSeparated();

  do {
    bool parsed_longhand[kMaxNumAnimationLonghands] = {false};
    do {
      bool found_property = false;
      for (size_t i = 0; i < longhand_count; ++i) {
        if (parsed_longhand[i])
          continue;

        CSSValue* value = consumeLonghandItem(shorthand.properties()[i], range,
                                              context, use_legacy_parsing);
        if (value) {
          parsed_longhand[i] = true;
          found_property = true;
          longhands[i]->Append(*value);
          break;
        }
      }
      if (!found_property)
        return false;
    } while (!range.AtEnd() && range.Peek().GetType() != kCommaToken);

    // TODO(timloh): This will make invalid longhands, see crbug.com/386459
    for (size_t i = 0; i < longhand_count; ++i) {
      if (!parsed_longhand[i])
        longhands[i]->Append(*CSSInitialValue::Create());
      parsed_longhand[i] = false;
    }
  } while (CSSPropertyParserHelpers::ConsumeCommaIncludingWhitespace(range));

  return true;
}

}  // namespace blink
