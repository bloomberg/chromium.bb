// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_API_STRING_ORDINAL_H_
#define SYNC_API_STRING_ORDINAL_H_

#include "base/basictypes.h"
#include "sync/internal_api/public/base/ordinal.h"

namespace syncer {

// A StringOrdinal is an Ordinal with range 'a'-'z' for printability
// of its internal byte string representation.  (Think of
// StringOrdinal as being short for PrintableStringOrdinal.)  It
// should be used for data types that want to maintain one or more
// orderings for nodes.
//
// Since StringOrdinals contain only printable characters, it is safe
// to store as a string in a protobuf.

struct StringOrdinalTraits {
  static const uint8 kZeroDigit = 'a';
  static const uint8 kMaxDigit = 'z';
  static const size_t kMinLength = 1;
};

typedef Ordinal<StringOrdinalTraits> StringOrdinal;

COMPILE_ASSERT(StringOrdinal::kZeroDigit == 'a',
               StringOrdinalHasCorrectZeroDigit);
COMPILE_ASSERT(StringOrdinal::kOneDigit == 'b',
               StringOrdinalHasCorrectOneDigit);
COMPILE_ASSERT(StringOrdinal::kMidDigit == 'n',
               StringOrdinalHasCorrectMidDigit);
COMPILE_ASSERT(StringOrdinal::kMaxDigit == 'z',
               StringOrdinalHasCorrectMaxDigit);
COMPILE_ASSERT(StringOrdinal::kMidDigitValue == 13,
               StringOrdinalHasCorrectMidDigitValue);
COMPILE_ASSERT(StringOrdinal::kMaxDigitValue == 25,
               StringOrdinalHasCorrectMaxDigitValue);
COMPILE_ASSERT(StringOrdinal::kRadix == 26,
               StringOrdinalHasCorrectRadix);

}  // namespace syncer

#endif  // SYNC_API_STRING_ORDINAL_H_
