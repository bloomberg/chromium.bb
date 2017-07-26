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
  static constexpr unsigned kAcceptMinusZeroForUnsigned = 1 << 3;

  // 'Strict' behavior for String/StringImpl.
  static constexpr unsigned kStrict =
      kAcceptLeadingPlus | kAcceptLeadingTrailingWhitespace;
  // Non-'Strict' behavior for String/StringImpl.
  static constexpr unsigned kLoose = kStrict | kAcceptTrailingGarbage;

  // This constructor allows implicit conversion from unsigned.
  NumberParsingOptions(unsigned options) : options_(options) {
    DCHECK_LT(options, 1u << 4) << "NumberParsingOptions should be built with "
                                   "a combination of "
                                   "NumberParsingOptions::kFoo constants.";
  }

  bool AcceptTrailingGarbage() const {
    return options_ & kAcceptTrailingGarbage;
  }
  bool AcceptLeadingPlus() const { return options_ & kAcceptLeadingPlus; }
  bool AcceptWhitespace() const {
    return options_ & kAcceptLeadingTrailingWhitespace;
  }
  bool AcceptMinusZeroForUnsigned() const {
    return options_ & kAcceptMinusZeroForUnsigned;
  }

 private:
  unsigned options_;
};

enum class NumberParsingResult {
  kSuccess,
  kError,
  // For UInt functions, kOverflowMin never happens. Negative numbers are
  // treated as kError. This behavior matches to the HTML standard.
  // https://html.spec.whatwg.org/multipage/common-microsyntaxes.html#rules-for-parsing-non-negative-integers
  kOverflowMin,
  kOverflowMax,
};

// string -> int.
WTF_EXPORT int CharactersToInt(const LChar*,
                               size_t,
                               NumberParsingOptions,
                               bool* ok);
WTF_EXPORT int CharactersToInt(const UChar*,
                               size_t,
                               NumberParsingOptions,
                               bool* ok);

// string -> unsigned.
WTF_EXPORT unsigned HexCharactersToUInt(const LChar*,
                                        size_t,
                                        NumberParsingOptions,
                                        bool* ok);
WTF_EXPORT unsigned HexCharactersToUInt(const UChar*,
                                        size_t,
                                        NumberParsingOptions,
                                        bool* ok);
WTF_EXPORT unsigned CharactersToUInt(const LChar*,
                                     size_t,
                                     NumberParsingOptions,
                                     bool* ok);
WTF_EXPORT unsigned CharactersToUInt(const UChar*,
                                     size_t,
                                     NumberParsingOptions,
                                     bool* ok);

// NumberParsingResult versions of CharactersToUInt. They can detect
// overflow. |NumberParsingResult*| should not be nullptr;
WTF_EXPORT unsigned CharactersToUInt(const LChar*,
                                     size_t,
                                     NumberParsingOptions,
                                     NumberParsingResult*);
WTF_EXPORT unsigned CharactersToUInt(const UChar*,
                                     size_t,
                                     NumberParsingOptions,
                                     NumberParsingResult*);

// string -> int64_t.
WTF_EXPORT int64_t CharactersToInt64(const LChar*,
                                     size_t,
                                     NumberParsingOptions,
                                     bool* ok);
WTF_EXPORT int64_t CharactersToInt64(const UChar*,
                                     size_t,
                                     NumberParsingOptions,
                                     bool* ok);

// string -> uint64_t.
WTF_EXPORT uint64_t CharactersToUInt64(const LChar*,
                                       size_t,
                                       NumberParsingOptions,
                                       bool* ok);
WTF_EXPORT uint64_t CharactersToUInt64(const UChar*,
                                       size_t,
                                       NumberParsingOptions,
                                       bool* ok);

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

using WTF::CharactersToInt;
using WTF::CharactersToUInt;
using WTF::CharactersToInt64;
using WTF::CharactersToUInt64;
using WTF::CharactersToDouble;
using WTF::CharactersToFloat;

#endif
