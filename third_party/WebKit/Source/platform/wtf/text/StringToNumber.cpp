// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/wtf/text/StringToNumber.h"

#include "platform/wtf/ASCIICType.h"
#include "platform/wtf/dtoa.h"
#include "platform/wtf/text/StringImpl.h"
#include <type_traits>

namespace WTF {

static bool IsCharacterAllowedInBase(UChar c, int base) {
  if (c > 0x7F)
    return false;
  if (IsASCIIDigit(c))
    return c - '0' < base;
  if (IsASCIIAlpha(c)) {
    if (base > 36)
      base = 36;
    return (c >= 'a' && c < 'a' + base - 10) ||
           (c >= 'A' && c < 'A' + base - 10);
  }
  return false;
}

template <typename IntegralType, typename CharType>
static inline IntegralType ToIntegralType(const CharType* data,
                                          size_t length,
                                          NumberParsingState* parsing_state,
                                          int base) {
  static_assert(std::is_integral<IntegralType>::value,
                "IntegralType must be an integral type.");
  static constexpr IntegralType kIntegralMax =
      std::numeric_limits<IntegralType>::max();
  static constexpr IntegralType kIntegralMin =
      std::numeric_limits<IntegralType>::min();
  static constexpr bool kIsSigned =
      std::numeric_limits<IntegralType>::is_signed;
  DCHECK(parsing_state);

  IntegralType value = 0;
  NumberParsingState state = NumberParsingState::kError;
  bool is_negative = false;
  bool overflow = false;

  if (!data)
    goto bye;

  // skip leading whitespace
  while (length && IsSpaceOrNewline(*data)) {
    --length;
    ++data;
  }

  if (kIsSigned && length && *data == '-') {
    --length;
    ++data;
    is_negative = true;
  } else if (length && *data == '+') {
    --length;
    ++data;
  }

  if (!length || !IsCharacterAllowedInBase(*data, base))
    goto bye;

  while (length && IsCharacterAllowedInBase(*data, base)) {
    --length;
    IntegralType digit_value;
    CharType c = *data;
    if (IsASCIIDigit(c))
      digit_value = c - '0';
    else if (c >= 'a')
      digit_value = c - 'a' + 10;
    else
      digit_value = c - 'A' + 10;

    if (is_negative) {
      // Overflow condition:
      //       value * base - digit_value < kIntegralMin
      //   <=> value < (kIntegralMin + digit_value) / base
      // We must be careful of rounding errors here, but the default rounding
      // mode (round to zero) works well, so we can use this formula as-is.
      if (value < (kIntegralMin + digit_value) / base) {
        state = NumberParsingState::kOverflowMin;
        overflow = true;
      }
    } else {
      // Overflow condition:
      //       value * base + digit_value > kIntegralMax
      //   <=> value > (kIntegralMax + digit_value) / base
      // Ditto regarding rounding errors.
      if (value > (kIntegralMax - digit_value) / base) {
        state = NumberParsingState::kOverflowMax;
        overflow = true;
      }
    }

    if (!overflow) {
      if (is_negative)
        value = base * value - digit_value;
      else
        value = base * value + digit_value;
    }
    ++data;
  }

  // skip trailing space
  while (length && IsSpaceOrNewline(*data)) {
    --length;
    ++data;
  }

  if (length == 0) {
    if (!overflow)
      state = NumberParsingState::kSuccess;
  } else {
    // Even if we detected overflow, we return kError for trailing garbage.
    state = NumberParsingState::kError;
  }
bye:
  *parsing_state = state;
  return state == NumberParsingState::kSuccess ? value : 0;
}

template <typename IntegralType, typename CharType>
static inline IntegralType ToIntegralType(const CharType* data,
                                          size_t length,
                                          bool* ok,
                                          int base) {
  NumberParsingState state;
  IntegralType value =
      ToIntegralType<IntegralType, CharType>(data, length, &state, base);
  if (ok)
    *ok = state == NumberParsingState::kSuccess;
  return value;
}

template <typename CharType>
static unsigned LengthOfCharactersAsInteger(const CharType* data,
                                            size_t length) {
  size_t i = 0;

  // Allow leading spaces.
  for (; i != length; ++i) {
    if (!IsSpaceOrNewline(data[i]))
      break;
  }

  // Allow sign.
  if (i != length && (data[i] == '+' || data[i] == '-'))
    ++i;

  // Allow digits.
  for (; i != length; ++i) {
    if (!IsASCIIDigit(data[i]))
      break;
  }

  return i;
}

unsigned CharactersToUIntStrict(const LChar* data,
                                size_t length,
                                NumberParsingState* state) {
  return ToIntegralType<unsigned, LChar>(data, length, state, 10);
}

unsigned CharactersToUIntStrict(const UChar* data,
                                size_t length,
                                NumberParsingState* state) {
  return ToIntegralType<unsigned, UChar>(data, length, state, 10);
}

int CharactersToIntStrict(const LChar* data,
                          size_t length,
                          bool* ok,
                          int base) {
  return ToIntegralType<int, LChar>(data, length, ok, base);
}

int CharactersToIntStrict(const UChar* data,
                          size_t length,
                          bool* ok,
                          int base) {
  return ToIntegralType<int, UChar>(data, length, ok, base);
}

unsigned CharactersToUIntStrict(const LChar* data,
                                size_t length,
                                bool* ok,
                                int base) {
  return ToIntegralType<unsigned, LChar>(data, length, ok, base);
}

unsigned CharactersToUIntStrict(const UChar* data,
                                size_t length,
                                bool* ok,
                                int base) {
  return ToIntegralType<unsigned, UChar>(data, length, ok, base);
}

int64_t CharactersToInt64Strict(const LChar* data,
                                size_t length,
                                bool* ok,
                                int base) {
  return ToIntegralType<int64_t, LChar>(data, length, ok, base);
}

int64_t CharactersToInt64Strict(const UChar* data,
                                size_t length,
                                bool* ok,
                                int base) {
  return ToIntegralType<int64_t, UChar>(data, length, ok, base);
}

uint64_t CharactersToUInt64Strict(const LChar* data,
                                  size_t length,
                                  bool* ok,
                                  int base) {
  return ToIntegralType<uint64_t, LChar>(data, length, ok, base);
}

uint64_t CharactersToUInt64Strict(const UChar* data,
                                  size_t length,
                                  bool* ok,
                                  int base) {
  return ToIntegralType<uint64_t, UChar>(data, length, ok, base);
}

int CharactersToInt(const LChar* data, size_t length, bool* ok) {
  return ToIntegralType<int, LChar>(
      data, LengthOfCharactersAsInteger<LChar>(data, length), ok, 10);
}

int CharactersToInt(const UChar* data, size_t length, bool* ok) {
  return ToIntegralType<int, UChar>(
      data, LengthOfCharactersAsInteger(data, length), ok, 10);
}

unsigned CharactersToUInt(const LChar* data, size_t length, bool* ok) {
  return ToIntegralType<unsigned, LChar>(
      data, LengthOfCharactersAsInteger<LChar>(data, length), ok, 10);
}

unsigned CharactersToUInt(const UChar* data, size_t length, bool* ok) {
  return ToIntegralType<unsigned, UChar>(
      data, LengthOfCharactersAsInteger<UChar>(data, length), ok, 10);
}

int64_t CharactersToInt64(const LChar* data, size_t length, bool* ok) {
  return ToIntegralType<int64_t, LChar>(
      data, LengthOfCharactersAsInteger<LChar>(data, length), ok, 10);
}

int64_t CharactersToInt64(const UChar* data, size_t length, bool* ok) {
  return ToIntegralType<int64_t, UChar>(
      data, LengthOfCharactersAsInteger<UChar>(data, length), ok, 10);
}

uint64_t CharactersToUInt64(const LChar* data, size_t length, bool* ok) {
  return ToIntegralType<uint64_t, LChar>(
      data, LengthOfCharactersAsInteger<LChar>(data, length), ok, 10);
}

uint64_t CharactersToUInt64(const UChar* data, size_t length, bool* ok) {
  return ToIntegralType<uint64_t, UChar>(
      data, LengthOfCharactersAsInteger<UChar>(data, length), ok, 10);
}

enum TrailingJunkPolicy { kDisallowTrailingJunk, kAllowTrailingJunk };

template <typename CharType, TrailingJunkPolicy policy>
static inline double ToDoubleType(const CharType* data,
                                  size_t length,
                                  bool* ok,
                                  size_t& parsed_length) {
  size_t leading_spaces_length = 0;
  while (leading_spaces_length < length &&
         IsASCIISpace(data[leading_spaces_length]))
    ++leading_spaces_length;

  double number = ParseDouble(data + leading_spaces_length,
                              length - leading_spaces_length, parsed_length);
  if (!parsed_length) {
    if (ok)
      *ok = false;
    return 0.0;
  }

  parsed_length += leading_spaces_length;
  if (ok)
    *ok = policy == kAllowTrailingJunk || parsed_length == length;
  return number;
}

double CharactersToDouble(const LChar* data, size_t length, bool* ok) {
  size_t parsed_length;
  return ToDoubleType<LChar, kDisallowTrailingJunk>(data, length, ok,
                                                    parsed_length);
}

double CharactersToDouble(const UChar* data, size_t length, bool* ok) {
  size_t parsed_length;
  return ToDoubleType<UChar, kDisallowTrailingJunk>(data, length, ok,
                                                    parsed_length);
}

double CharactersToDouble(const LChar* data,
                          size_t length,
                          size_t& parsed_length) {
  return ToDoubleType<LChar, kAllowTrailingJunk>(data, length, nullptr,
                                                 parsed_length);
}

double CharactersToDouble(const UChar* data,
                          size_t length,
                          size_t& parsed_length) {
  return ToDoubleType<UChar, kAllowTrailingJunk>(data, length, nullptr,
                                                 parsed_length);
}

float CharactersToFloat(const LChar* data, size_t length, bool* ok) {
  // FIXME: This will return ok even when the string fits into a double but
  // not a float.
  size_t parsed_length;
  return static_cast<float>(ToDoubleType<LChar, kDisallowTrailingJunk>(
      data, length, ok, parsed_length));
}

float CharactersToFloat(const UChar* data, size_t length, bool* ok) {
  // FIXME: This will return ok even when the string fits into a double but
  // not a float.
  size_t parsed_length;
  return static_cast<float>(ToDoubleType<UChar, kDisallowTrailingJunk>(
      data, length, ok, parsed_length));
}

float CharactersToFloat(const LChar* data,
                        size_t length,
                        size_t& parsed_length) {
  // FIXME: This will return ok even when the string fits into a double but
  // not a float.
  return static_cast<float>(
      ToDoubleType<LChar, kAllowTrailingJunk>(data, length, 0, parsed_length));
}

float CharactersToFloat(const UChar* data,
                        size_t length,
                        size_t& parsed_length) {
  // FIXME: This will return ok even when the string fits into a double but
  // not a float.
  return static_cast<float>(
      ToDoubleType<UChar, kAllowTrailingJunk>(data, length, 0, parsed_length));
}

}  // namespace WTF
