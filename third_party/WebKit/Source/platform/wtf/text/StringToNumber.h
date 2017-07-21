// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WTF_StringToNumber_h
#define WTF_StringToNumber_h

#include "platform/wtf/WTFExport.h"
#include "platform/wtf/text/Unicode.h"

namespace WTF {

// Copyable and immutable object representing number parsing flags.
class NumberParsingOptions {
 public:
  static constexpr unsigned kNone = 0;
  static constexpr unsigned kAcceptTrailingGarbage = 1;
  static constexpr unsigned kAcceptLeadingPlus = 1 << 1;
  static constexpr unsigned kAcceptLeadingTrailingWhitespace = 1 << 2;
  // TODO(tkent): Add kAcceptMinusZeroForUnsigned

  // Legacy 'Strict' behavior.
  static constexpr unsigned kStrict =
      kAcceptLeadingPlus | kAcceptLeadingTrailingWhitespace;
  // Legacy non-'Strict' behavior.
  static constexpr unsigned kLoose = kStrict | kAcceptTrailingGarbage;

  // This constructor allows implicit conversion from unsigned.
  NumberParsingOptions(unsigned options) : options_(options) {}

  bool AcceptTrailingGarbage() const {
    return options_ & kAcceptTrailingGarbage;
  }
  bool AcceptLeadingPlus() const { return options_ & kAcceptLeadingPlus; }
  bool AcceptWhitespace() const {
    return options_ & kAcceptLeadingTrailingWhitespace;
  }

 private:
  unsigned options_;
};

// TODO(tkent): Merge CharactersToFooStrict and CharactersToFoo by adding
// NumberParsingOptions argument.

// string -> int.
WTF_EXPORT int CharactersToIntStrict(const LChar*, size_t, bool* ok);
WTF_EXPORT int CharactersToIntStrict(const UChar*, size_t, bool* ok);
// The following two functions ignore trailing garbage.
WTF_EXPORT int CharactersToInt(const LChar*, size_t, bool* ok);
WTF_EXPORT int CharactersToInt(const UChar*, size_t, bool* ok);

enum class NumberParsingState {
  kSuccess,
  kError,
  // For UInt functions, kOverflowMin never happens. Negative numbers are
  // treated as kError. This behavior matches to the HTML standard.
  // https://html.spec.whatwg.org/multipage/common-microsyntaxes.html#rules-for-parsing-non-negative-integers
  kOverflowMin,
  kOverflowMax,
};

// string -> unsigned.
// These functions do not accept "-0".
WTF_EXPORT unsigned CharactersToUIntStrict(const LChar*, size_t, bool* ok);
WTF_EXPORT unsigned CharactersToUIntStrict(const UChar*, size_t, bool* ok);
WTF_EXPORT unsigned HexCharactersToUIntStrict(const LChar*, size_t, bool* ok);
WTF_EXPORT unsigned HexCharactersToUIntStrict(const UChar*, size_t, bool* ok);
// The following two functions ignore trailing garbage.
WTF_EXPORT unsigned CharactersToUInt(const LChar*, size_t, bool* ok);
WTF_EXPORT unsigned CharactersToUInt(const UChar*, size_t, bool* ok);

// NumberParsingState versions of CharactersToUIntStrict. They can detect
// overflow. |NumberParsingState*| should not be nullptr;
WTF_EXPORT unsigned CharactersToUIntStrict(const LChar*,
                                           size_t,
                                           NumberParsingState*);
WTF_EXPORT unsigned CharactersToUIntStrict(const UChar*,
                                           size_t,
                                           NumberParsingState*);

// string -> int64_t.
WTF_EXPORT int64_t CharactersToInt64Strict(const LChar*, size_t, bool* ok);
WTF_EXPORT int64_t CharactersToInt64Strict(const UChar*, size_t, bool* ok);
// The following two functions ignore trailing garbage.
WTF_EXPORT int64_t CharactersToInt64(const LChar*, size_t, bool* ok);
WTF_EXPORT int64_t CharactersToInt64(const UChar*, size_t, bool* ok);

// string -> uint64_t.
// These functions do not accept "-0".
WTF_EXPORT uint64_t CharactersToUInt64Strict(const LChar*, size_t, bool* ok);
WTF_EXPORT uint64_t CharactersToUInt64Strict(const UChar*, size_t, bool* ok);
// The following two functions ignore trailing garbage.
WTF_EXPORT uint64_t CharactersToUInt64(const LChar*, size_t, bool* ok);
WTF_EXPORT uint64_t CharactersToUInt64(const UChar*, size_t, bool* ok);

// FIXME: Like the strict functions above, these give false for "ok" when there
// is trailing garbage.  Like the non-strict functions above, these return the
// value when there is trailing garbage.  It would be better if these were more
// consistent with the above functions instead.
//
// string -> double.
WTF_EXPORT double CharactersToDouble(const LChar*, size_t, bool* ok);
WTF_EXPORT double CharactersToDouble(const UChar*, size_t, bool* ok);
WTF_EXPORT double CharactersToDouble(const LChar*,
                                     size_t,
                                     size_t& parsed_length);
WTF_EXPORT double CharactersToDouble(const UChar*,
                                     size_t,
                                     size_t& parsed_length);

// string -> float.
WTF_EXPORT float CharactersToFloat(const LChar*, size_t, bool* ok);
WTF_EXPORT float CharactersToFloat(const UChar*, size_t, bool* ok);
WTF_EXPORT float CharactersToFloat(const LChar*, size_t, size_t& parsed_length);
WTF_EXPORT float CharactersToFloat(const UChar*, size_t, size_t& parsed_length);

}  // namespace WTF

using WTF::CharactersToIntStrict;
using WTF::CharactersToUIntStrict;
using WTF::CharactersToInt64Strict;
using WTF::CharactersToUInt64Strict;
using WTF::CharactersToInt;
using WTF::CharactersToUInt;
using WTF::CharactersToInt64;
using WTF::CharactersToUInt64;
using WTF::CharactersToDouble;
using WTF::CharactersToFloat;

#endif
