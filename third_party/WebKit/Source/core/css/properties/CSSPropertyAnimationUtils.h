// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPropertyAnimationUtils_h
#define CSSPropertyAnimationUtils_h

#include "core/StylePropertyShorthand.h"
#include "core/css/CSSInitialValue.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "platform/heap/HeapAllocator.h"
#include "platform/wtf/Allocator.h"

namespace blink {

constexpr size_t kMaxNumAnimationLonghands = 8;

class CSSPropertyAnimationUtils {
  STATIC_ONLY(CSSPropertyAnimationUtils);

 public:
  template <typename Callback, typename... Args>
  static bool ConsumeAnimationShorthand(
      const StylePropertyShorthand& shorthand,
      HeapVector<CSSValueList*, kMaxNumAnimationLonghands>& longhands,
      Callback consumeLonghandItem,
      CSSParserTokenRange& range,
      const Args&... args) {
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

          CSSValue* value =
              consumeLonghandItem(shorthand.properties()[i], range, args...);
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
};

}  // namespace blink

#endif  // CSSPropertyAnimationUtils_h
