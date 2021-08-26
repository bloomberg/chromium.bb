// Copyright 2020 The Tint Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/reader/wgsl/lexer.h"

#include <cmath>
#include <cstring>
#include <limits>
#include <utility>

#include "src/debug.h"

namespace tint {
namespace reader {
namespace wgsl {
namespace {

bool is_whitespace(char c) {
  return std::isspace(c);
}

uint32_t dec_value(char c) {
  if (c >= '0' && c <= '9') {
    return static_cast<uint32_t>(c - '0');
  }
  return 0;
}

uint32_t hex_value(char c) {
  if (c >= '0' && c <= '9') {
    return static_cast<uint32_t>(c - '0');
  }
  if (c >= 'a' && c <= 'f') {
    return 0xA + static_cast<uint32_t>(c - 'a');
  }
  if (c >= 'A' && c <= 'F') {
    return 0xA + static_cast<uint32_t>(c - 'A');
  }
  return 0;
}

}  // namespace

Lexer::Lexer(const std::string& file_path, const Source::FileContent* content)
    : file_path_(file_path),
      content_(content),
      len_(static_cast<uint32_t>(content->data.size())),
      location_{1, 1} {}

Lexer::~Lexer() = default;

Token Lexer::next() {
  skip_whitespace();
  skip_comments();

  if (is_eof()) {
    return {Token::Type::kEOF, begin_source()};
  }

  auto t = try_hex_float();
  if (!t.IsUninitialized()) {
    return t;
  }

  t = try_hex_integer();
  if (!t.IsUninitialized()) {
    return t;
  }

  t = try_float();
  if (!t.IsUninitialized()) {
    return t;
  }

  t = try_integer();
  if (!t.IsUninitialized()) {
    return t;
  }

  t = try_punctuation();
  if (!t.IsUninitialized()) {
    return t;
  }

  t = try_ident();
  if (!t.IsUninitialized()) {
    return t;
  }

  return {Token::Type::kError, begin_source(), "invalid character found"};
}

Source Lexer::begin_source() const {
  Source src{};
  src.file_path = file_path_;
  src.file_content = content_;
  src.range.begin = location_;
  src.range.end = location_;
  return src;
}

void Lexer::end_source(Source& src) const {
  src.range.end = location_;
}

bool Lexer::is_eof() const {
  return pos_ >= len_;
}

bool Lexer::is_alpha(char ch) const {
  return std::isalpha(ch) || ch == '_';
}

bool Lexer::is_digit(char ch) const {
  return std::isdigit(ch);
}

bool Lexer::is_alphanum(char ch) const {
  return is_alpha(ch) || is_digit(ch);
}

bool Lexer::is_hex(char ch) const {
  return std::isxdigit(ch);
}

bool Lexer::matches(size_t pos, const std::string& substr) {
  if (pos >= len_)
    return false;
  return content_->data.substr(pos, substr.size()) == substr;
}

void Lexer::skip_whitespace() {
  for (;;) {
    auto pos = pos_;
    while (!is_eof() && is_whitespace(content_->data[pos_])) {
      if (matches(pos_, "\n")) {
        pos_++;
        location_.line++;
        location_.column = 1;
        continue;
      }

      pos_++;
      location_.column++;
    }

    skip_comments();

    // If the cursor didn't advance we didn't remove any whitespace
    // so we're done.
    if (pos == pos_)
      break;
  }
}

void Lexer::skip_comments() {
  if (matches(pos_, "//")) {
    // Line comment: ignore everything until the end of line.
    while (!is_eof() && !matches(pos_, "\n")) {
      pos_++;
      location_.column++;
    }
    return;
  }

  if (matches(pos_, "/*")) {
    // Block comment: ignore everything until the closing '*/' token.
    pos_ += 2;
    location_.column += 2;

    int depth = 1;
    while (!is_eof() && depth > 0) {
      if (matches(pos_, "/*")) {
        // Start of block comment: increase nesting depth.
        pos_ += 2;
        location_.column += 2;
        depth++;
      } else if (matches(pos_, "*/")) {
        // End of block comment: decrease nesting depth.
        pos_ += 2;
        location_.column += 2;
        depth--;
      } else if (matches(pos_, "\n")) {
        // Newline: skip and update source location.
        pos_++;
        location_.line++;
        location_.column = 1;
      } else {
        // Anything else: skip and update source location.
        pos_++;
        location_.column++;
      }
    }
  }
}

Token Lexer::try_float() {
  auto start = pos_;
  auto end = pos_;

  auto source = begin_source();

  if (matches(end, "-")) {
    end++;
  }
  while (end < len_ && is_digit(content_->data[end])) {
    end++;
  }

  if (end >= len_ || !matches(end, ".")) {
    return {};
  }
  end++;

  while (end < len_ && is_digit(content_->data[end])) {
    end++;
  }

  // Parse the exponent if one exists
  if (end < len_ && matches(end, "e")) {
    end++;
    if (end < len_ && (matches(end, "+") || matches(end, "-"))) {
      end++;
    }

    auto exp_start = end;
    while (end < len_ && isdigit(content_->data[end])) {
      end++;
    }

    // Must have an exponent
    if (exp_start == end)
      return {};
  }

  auto str = content_->data.substr(start, end - start);
  if (str == "." || str == "-.")
    return {};

  pos_ = end;
  location_.column += (end - start);

  end_source(source);

  auto res = strtod(content_->data.c_str() + start, nullptr);
  // This handles if the number is a really small in the exponent
  if (res > 0 && res < static_cast<double>(std::numeric_limits<float>::min())) {
    return {Token::Type::kError, source, "f32 (" + str + " too small"};
  }
  // This handles if the number is really large negative number
  if (res < static_cast<double>(std::numeric_limits<float>::lowest())) {
    return {Token::Type::kError, source, "f32 (" + str + ") too small"};
  }
  if (res > static_cast<double>(std::numeric_limits<float>::max())) {
    return {Token::Type::kError, source, "f32 (" + str + ") too large"};
  }

  return {source, static_cast<float>(res)};
}

Token Lexer::try_hex_float() {
  constexpr uint32_t kTotalBits = 32;
  constexpr uint32_t kTotalMsb = kTotalBits - 1;
  constexpr uint32_t kMantissaBits = 23;
  constexpr uint32_t kMantissaMsb = kMantissaBits - 1;
  constexpr uint32_t kMantissaShiftRight = kTotalBits - kMantissaBits;
  constexpr int32_t kExponentBias = 127;
  constexpr int32_t kExponentMax = 255;
  constexpr uint32_t kExponentBits = 8;
  constexpr uint32_t kExponentMask = (1 << kExponentBits) - 1;
  constexpr uint32_t kExponentLeftShift = kMantissaBits;
  constexpr uint32_t kSignBit = 31;

  auto start = pos_;
  auto end = pos_;

  auto source = begin_source();

  // clang-format off
  // -?0x([0-9a-fA-F]*.?[0-9a-fA-F]+ | [0-9a-fA-F]+.[0-9a-fA-F]*)(p|P)(+|-)?[0-9]+  // NOLINT
  // clang-format on

  // -?
  int32_t sign_bit = 0;
  if (matches(end, "-")) {
    sign_bit = 1;
    end++;
  }
  // 0x
  if (matches(end, "0x")) {
    end += 2;
  } else {
    return {};
  }

  uint32_t mantissa = 0;
  int32_t exponent = 0;

  // `set_next_mantissa_bit_to` sets next `mantissa` bit starting from msb to
  // lsb to value 1 if `set` is true, 0 otherwise
  uint32_t mantissa_next_bit = kTotalMsb;
  auto set_next_mantissa_bit_to = [&](bool set, bool integer_part) -> bool {
    // If adding bits for the integer part, we can overflow whether we set the
    // bit or not. For the fractional part, we can only overflow when setting
    // the bit.
    const bool check_overflow = integer_part || set;
    if (check_overflow && (mantissa_next_bit > kTotalMsb)) {
      return false;  // Overflowed mantissa
    }
    if (set) {
      mantissa |= (1 << mantissa_next_bit);
    }
    --mantissa_next_bit;
    return true;
  };

  // Collect integer range (if any)
  auto integer_range = std::make_pair(end, end);
  while (end < len_ && is_hex(content_->data[end])) {
    integer_range.second = ++end;
  }

  // .?
  if (matches(end, ".")) {
    end++;
  }

  // Collect fractional range (if any)
  auto fractional_range = std::make_pair(end, end);
  while (end < len_ && is_hex(content_->data[end])) {
    fractional_range.second = ++end;
  }

  // Must have at least an integer or fractional part
  if ((integer_range.first == integer_range.second) &&
      (fractional_range.first == fractional_range.second)) {
    return {};
  }

  // (p|P)
  if (matches(end, "p") || matches(end, "P")) {
    end++;
  } else {
    return {};
  }

  // At this point, we know for sure our token is a hex float value.

  // Parse integer part
  // [0-9a-fA-F]*
  bool has_zero_integer = true;
  bool leading_bit_seen = false;
  for (auto i = integer_range.first; i < integer_range.second; ++i) {
    const auto nibble = hex_value(content_->data[i]);
    if (nibble != 0) {
      has_zero_integer = false;
    }

    for (int32_t bit = 3; bit >= 0; --bit) {
      auto v = 1 & (nibble >> bit);

      // Skip leading 0s and the first 1
      if (leading_bit_seen) {
        if (!set_next_mantissa_bit_to(v != 0, true)) {
          return {Token::Type::kError, source,
                  "mantissa is too large for hex float"};
        }
        ++exponent;
      } else {
        if (v == 1) {
          leading_bit_seen = true;
        }
      }
    }
  }

  // Parse fractional part
  // [0-9a-fA-F]*
  leading_bit_seen = false;
  for (auto i = fractional_range.first; i < fractional_range.second; ++i) {
    auto nibble = hex_value(content_->data[i]);
    for (int32_t bit = 3; bit >= 0; --bit) {
      auto v = 1 & (nibble >> bit);

      if (v == 1) {
        leading_bit_seen = true;
      }

      // If integer part is 0 (denorm), we only start writing bits to the
      // mantissa once we have a non-zero fractional bit. While the fractional
      // values are 0, we adjust the exponent to avoid overflowing `mantissa`.
      if (has_zero_integer && !leading_bit_seen) {
        --exponent;
      } else {
        if (!set_next_mantissa_bit_to(v != 0, false)) {
          return {Token::Type::kError, source,
                  "mantissa is too large for hex float"};
        }
      }
    }
  }

  // (+|-)?
  int32_t exponent_sign = 1;
  if (matches(end, "+")) {
    end++;
  } else if (matches(end, "-")) {
    exponent_sign = -1;
    end++;
  }

  // Parse exponent from input
  // [0-9]+
  bool has_exponent = false;
  uint32_t input_exponent = 0;
  while (end < len_ && isdigit(content_->data[end])) {
    has_exponent = true;
    auto prev_exponent = input_exponent;
    input_exponent = (input_exponent * 10) + dec_value(content_->data[end]);
    if (prev_exponent > input_exponent) {
      return {Token::Type::kError, source,
              "exponent is too large for hex float"};
    }
    end++;
  }
  if (!has_exponent) {
    return {Token::Type::kError, source,
            "expected an exponent value for hex float"};
  }

  pos_ = end;
  location_.column += (end - start);
  end_source(source);

  // Compute exponent so far
  exponent = exponent + (input_exponent * exponent_sign);

  // Determine if value is zero
  // Note: it's not enough to check mantissa == 0 as we drop initial bit from
  // integer part.
  bool is_zero = has_zero_integer && mantissa == 0;
  TINT_ASSERT(Reader, !is_zero || mantissa == 0);

  if (is_zero) {
    // If value is zero, then ignore the exponent and produce a zero
    exponent = 0;
  } else {
    // Bias exponent if non-zero
    // After this, if exponent is <= 0, our value is a denormal
    exponent += kExponentBias;

    // Denormal uses biased exponent of -126, not -127
    if (has_zero_integer) {
      mantissa <<= 1;
      --exponent;
    }
  }

  // Shift mantissa to occupy the low 23 bits
  mantissa >>= kMantissaShiftRight;

  // If denormal, shift mantissa until our exponent is zero
  if (!is_zero) {
    // Denorm has exponent 0 and non-zero mantissa. We set the top bit here,
    // then shift the mantissa to make exponent zero.
    if (exponent <= 0) {
      mantissa >>= 1;
      mantissa |= (1 << kMantissaMsb);
    }

    while (exponent < 0) {
      mantissa >>= 1;
      ++exponent;

      // If underflow, clamp to zero
      if (mantissa == 0) {
        exponent = 0;
      }
    }
  }

  if (exponent > kExponentMax) {
    // Overflow: set to infinity
    exponent = kExponentMax;
    mantissa = 0;
  } else if (exponent == kExponentMax && mantissa != 0) {
    // NaN: set to infinity
    mantissa = 0;
  }

  // Combine sign, mantissa, and exponent
  uint32_t result_u32 = sign_bit << kSignBit;
  result_u32 |= mantissa;
  result_u32 |= (exponent & kExponentMask) << kExponentLeftShift;

  // Reinterpret as float and return
  float result;
  std::memcpy(&result, &result_u32, sizeof(result));
  return {source, static_cast<float>(result)};
}

Token Lexer::build_token_from_int_if_possible(Source source,
                                              size_t start,
                                              size_t end,
                                              int32_t base) {
  auto res = strtoll(content_->data.c_str() + start, nullptr, base);
  if (matches(pos_, "u")) {
    if (static_cast<uint64_t>(res) >
        static_cast<uint64_t>(std::numeric_limits<uint32_t>::max())) {
      return {
          Token::Type::kError, source,
          "u32 (" + content_->data.substr(start, end - start) + ") too large"};
    }
    pos_ += 1;
    location_.column += 1;
    end_source(source);
    return {source, static_cast<uint32_t>(res)};
  }

  if (res < static_cast<int64_t>(std::numeric_limits<int32_t>::min())) {
    return {
        Token::Type::kError, source,
        "i32 (" + content_->data.substr(start, end - start) + ") too small"};
  }
  if (res > static_cast<int64_t>(std::numeric_limits<int32_t>::max())) {
    return {
        Token::Type::kError, source,
        "i32 (" + content_->data.substr(start, end - start) + ") too large"};
  }
  end_source(source);
  return {source, static_cast<int32_t>(res)};
}

Token Lexer::try_hex_integer() {
  auto start = pos_;
  auto end = pos_;

  auto source = begin_source();

  if (matches(end, "-")) {
    end++;
  }
  if (!matches(end, "0x")) {
    return Token();
  }
  end += 2;

  while (!is_eof() && is_hex(content_->data[end])) {
    end += 1;
  }

  pos_ = end;
  location_.column += (end - start);

  return build_token_from_int_if_possible(source, start, end, 16);
}

Token Lexer::try_integer() {
  auto start = pos_;
  auto end = start;

  auto source = begin_source();

  if (matches(end, "-")) {
    end++;
  }
  if (end >= len_ || !is_digit(content_->data[end])) {
    return {};
  }

  auto first = end;
  while (end < len_ && is_digit(content_->data[end])) {
    end++;
  }

  // If the first digit is a zero this must only be zero as leading zeros
  // are not allowed.
  if (content_->data[first] == '0' && (end - first != 1))
    return {};

  pos_ = end;
  location_.column += (end - start);

  return build_token_from_int_if_possible(source, start, end, 10);
}

Token Lexer::try_ident() {
  // Must begin with an a-zA-Z_
  if (!is_alpha(content_->data[pos_])) {
    return {};
  }

  auto source = begin_source();

  auto s = pos_;
  while (!is_eof() && is_alphanum(content_->data[pos_])) {
    pos_++;
    location_.column++;
  }

  auto str = content_->data.substr(s, pos_ - s);
  end_source(source);

  auto t = check_keyword(source, str);
  if (!t.IsUninitialized()) {
    return t;
  }

  return {Token::Type::kIdentifier, source, str};
}

Token Lexer::try_punctuation() {
  auto source = begin_source();
  auto type = Token::Type::kUninitialized;

  if (matches(pos_, "[[")) {
    type = Token::Type::kAttrLeft;
    pos_ += 2;
    location_.column += 2;
  } else if (matches(pos_, "]]")) {
    type = Token::Type::kAttrRight;
    pos_ += 2;
    location_.column += 2;
  } else if (matches(pos_, "(")) {
    type = Token::Type::kParenLeft;
    pos_ += 1;
    location_.column += 1;
  } else if (matches(pos_, ")")) {
    type = Token::Type::kParenRight;
    pos_ += 1;
    location_.column += 1;
  } else if (matches(pos_, "[")) {
    type = Token::Type::kBracketLeft;
    pos_ += 1;
    location_.column += 1;
  } else if (matches(pos_, "]")) {
    type = Token::Type::kBracketRight;
    pos_ += 1;
    location_.column += 1;
  } else if (matches(pos_, "{")) {
    type = Token::Type::kBraceLeft;
    pos_ += 1;
    location_.column += 1;
  } else if (matches(pos_, "}")) {
    type = Token::Type::kBraceRight;
    pos_ += 1;
    location_.column += 1;
  } else if (matches(pos_, "&&")) {
    type = Token::Type::kAndAnd;
    pos_ += 2;
    location_.column += 2;
  } else if (matches(pos_, "&")) {
    type = Token::Type::kAnd;
    pos_ += 1;
    location_.column += 1;
  } else if (matches(pos_, "/")) {
    type = Token::Type::kForwardSlash;
    pos_ += 1;
    location_.column += 1;
  } else if (matches(pos_, "!=")) {
    type = Token::Type::kNotEqual;
    pos_ += 2;
    location_.column += 2;
  } else if (matches(pos_, "!")) {
    type = Token::Type::kBang;
    pos_ += 1;
    location_.column += 1;
  } else if (matches(pos_, ":")) {
    type = Token::Type::kColon;
    pos_ += 1;
    location_.column += 1;
  } else if (matches(pos_, ",")) {
    type = Token::Type::kComma;
    pos_ += 1;
    location_.column += 1;
  } else if (matches(pos_, "==")) {
    type = Token::Type::kEqualEqual;
    pos_ += 2;
    location_.column += 2;
  } else if (matches(pos_, "=")) {
    type = Token::Type::kEqual;
    pos_ += 1;
    location_.column += 1;
  } else if (matches(pos_, ">=")) {
    type = Token::Type::kGreaterThanEqual;
    pos_ += 2;
    location_.column += 2;
  } else if (matches(pos_, ">>")) {
    type = Token::Type::kShiftRight;
    pos_ += 2;
    location_.column += 2;
  } else if (matches(pos_, ">")) {
    type = Token::Type::kGreaterThan;
    pos_ += 1;
    location_.column += 1;
  } else if (matches(pos_, "<=")) {
    type = Token::Type::kLessThanEqual;
    pos_ += 2;
    location_.column += 2;
  } else if (matches(pos_, "<<")) {
    type = Token::Type::kShiftLeft;
    pos_ += 2;
    location_.column += 2;
  } else if (matches(pos_, "<")) {
    type = Token::Type::kLessThan;
    pos_ += 1;
    location_.column += 1;
  } else if (matches(pos_, "%")) {
    type = Token::Type::kMod;
    pos_ += 1;
    location_.column += 1;
  } else if (matches(pos_, "->")) {
    type = Token::Type::kArrow;
    pos_ += 2;
    location_.column += 2;
  } else if (matches(pos_, "--")) {
    type = Token::Type::kMinusMinus;
    pos_ += 2;
    location_.column += 2;
  } else if (matches(pos_, "-")) {
    type = Token::Type::kMinus;
    pos_ += 1;
    location_.column += 1;
  } else if (matches(pos_, ".")) {
    type = Token::Type::kPeriod;
    pos_ += 1;
    location_.column += 1;
  } else if (matches(pos_, "++")) {
    type = Token::Type::kPlusPlus;
    pos_ += 2;
    location_.column += 2;
  } else if (matches(pos_, "+")) {
    type = Token::Type::kPlus;
    pos_ += 1;
    location_.column += 1;
  } else if (matches(pos_, "||")) {
    type = Token::Type::kOrOr;
    pos_ += 2;
    location_.column += 2;
  } else if (matches(pos_, "|")) {
    type = Token::Type::kOr;
    pos_ += 1;
    location_.column += 1;
  } else if (matches(pos_, ";")) {
    type = Token::Type::kSemicolon;
    pos_ += 1;
    location_.column += 1;
  } else if (matches(pos_, "*")) {
    type = Token::Type::kStar;
    pos_ += 1;
    location_.column += 1;
  } else if (matches(pos_, "~")) {
    type = Token::Type::kTilde;
    pos_ += 1;
    location_.column += 1;
  } else if (matches(pos_, "^")) {
    type = Token::Type::kXor;
    pos_ += 1;
    location_.column += 1;
  }

  end_source(source);

  return {type, source};
}

Token Lexer::check_keyword(const Source& source, const std::string& str) {
  if (str == "array")
    return {Token::Type::kArray, source, "array"};
  if (str == "atomic")
    return {Token::Type::kAtomic, source, "atomic"};
  if (str == "bitcast")
    return {Token::Type::kBitcast, source, "bitcast"};
  if (str == "bool")
    return {Token::Type::kBool, source, "bool"};
  if (str == "break")
    return {Token::Type::kBreak, source, "break"};
  if (str == "case")
    return {Token::Type::kCase, source, "case"};
  if (str == "continue")
    return {Token::Type::kContinue, source, "continue"};
  if (str == "continuing")
    return {Token::Type::kContinuing, source, "continuing"};
  if (str == "discard")
    return {Token::Type::kDiscard, source, "discard"};
  if (str == "default")
    return {Token::Type::kDefault, source, "default"};
  if (str == "else")
    return {Token::Type::kElse, source, "else"};
  if (str == "elseif")
    return {Token::Type::kElseIf, source, "elseif"};
  if (str == "f32")
    return {Token::Type::kF32, source, "f32"};
  if (str == "fallthrough")
    return {Token::Type::kFallthrough, source, "fallthrough"};
  if (str == "false")
    return {Token::Type::kFalse, source, "false"};
  if (str == "fn")
    return {Token::Type::kFn, source, "fn"};
  if (str == "for")
    return {Token::Type::kFor, source, "for"};
  if (str == "bgra8unorm")
    return {Token::Type::kFormatBgra8Unorm, source, "bgra8unorm"};
  if (str == "bgra8unorm_srgb")
    return {Token::Type::kFormatBgra8UnormSrgb, source, "bgra8unorm_srgb"};
  if (str == "r16float")
    return {Token::Type::kFormatR16Float, source, "r16float"};
  if (str == "r16sint")
    return {Token::Type::kFormatR16Sint, source, "r16sint"};
  if (str == "r16uint")
    return {Token::Type::kFormatR16Uint, source, "r16uint"};
  if (str == "r32float")
    return {Token::Type::kFormatR32Float, source, "r32float"};
  if (str == "r32sint")
    return {Token::Type::kFormatR32Sint, source, "r32sint"};
  if (str == "r32uint")
    return {Token::Type::kFormatR32Uint, source, "r32uint"};
  if (str == "r8sint")
    return {Token::Type::kFormatR8Sint, source, "r8sint"};
  if (str == "r8snorm")
    return {Token::Type::kFormatR8Snorm, source, "r8snorm"};
  if (str == "r8uint")
    return {Token::Type::kFormatR8Uint, source, "r8uint"};
  if (str == "r8unorm")
    return {Token::Type::kFormatR8Unorm, source, "r8unorm"};
  if (str == "rg11b10float")
    return {Token::Type::kFormatRg11B10Float, source, "rg11b10float"};
  if (str == "rg16float")
    return {Token::Type::kFormatRg16Float, source, "rg16float"};
  if (str == "rg16sint")
    return {Token::Type::kFormatRg16Sint, source, "rg16sint"};
  if (str == "rg16uint")
    return {Token::Type::kFormatRg16Uint, source, "rg16uint"};
  if (str == "rg32float")
    return {Token::Type::kFormatRg32Float, source, "rg32float"};
  if (str == "rg32sint")
    return {Token::Type::kFormatRg32Sint, source, "rg32sint"};
  if (str == "rg32uint")
    return {Token::Type::kFormatRg32Uint, source, "rg32uint"};
  if (str == "rg8sint")
    return {Token::Type::kFormatRg8Sint, source, "rg8sint"};
  if (str == "rg8snorm")
    return {Token::Type::kFormatRg8Snorm, source, "rg8snorm"};
  if (str == "rg8uint")
    return {Token::Type::kFormatRg8Uint, source, "rg8uint"};
  if (str == "rg8unorm")
    return {Token::Type::kFormatRg8Unorm, source, "rg8unorm"};
  if (str == "rgb10a2unorm")
    return {Token::Type::kFormatRgb10A2Unorm, source, "rgb10a2unorm"};
  if (str == "rgba16float")
    return {Token::Type::kFormatRgba16Float, source, "rgba16float"};
  if (str == "rgba16sint")
    return {Token::Type::kFormatRgba16Sint, source, "rgba16sint"};
  if (str == "rgba16uint")
    return {Token::Type::kFormatRgba16Uint, source, "rgba16uint"};
  if (str == "rgba32float")
    return {Token::Type::kFormatRgba32Float, source, "rgba32float"};
  if (str == "rgba32sint")
    return {Token::Type::kFormatRgba32Sint, source, "rgba32sint"};
  if (str == "rgba32uint")
    return {Token::Type::kFormatRgba32Uint, source, "rgba32uint"};
  if (str == "rgba8sint")
    return {Token::Type::kFormatRgba8Sint, source, "rgba8sint"};
  if (str == "rgba8snorm")
    return {Token::Type::kFormatRgba8Snorm, source, "rgba8snorm"};
  if (str == "rgba8uint")
    return {Token::Type::kFormatRgba8Uint, source, "rgba8uint"};
  if (str == "rgba8unorm")
    return {Token::Type::kFormatRgba8Unorm, source, "rgba8unorm"};
  if (str == "rgba8unorm_srgb")
    return {Token::Type::kFormatRgba8UnormSrgb, source, "rgba8unorm_srgb"};
  if (str == "function")
    return {Token::Type::kFunction, source, "function"};
  if (str == "i32")
    return {Token::Type::kI32, source, "i32"};
  if (str == "if")
    return {Token::Type::kIf, source, "if"};
  if (str == "image")
    return {Token::Type::kImage, source, "image"};
  if (str == "import")
    return {Token::Type::kImport, source, "import"};
  if (str == "let")
    return {Token::Type::kLet, source, "let"};
  if (str == "loop")
    return {Token::Type::kLoop, source, "loop"};
  if (str == "mat2x2")
    return {Token::Type::kMat2x2, source, "mat2x2"};
  if (str == "mat2x3")
    return {Token::Type::kMat2x3, source, "mat2x3"};
  if (str == "mat2x4")
    return {Token::Type::kMat2x4, source, "mat2x4"};
  if (str == "mat3x2")
    return {Token::Type::kMat3x2, source, "mat3x2"};
  if (str == "mat3x3")
    return {Token::Type::kMat3x3, source, "mat3x3"};
  if (str == "mat3x4")
    return {Token::Type::kMat3x4, source, "mat3x4"};
  if (str == "mat4x2")
    return {Token::Type::kMat4x2, source, "mat4x2"};
  if (str == "mat4x3")
    return {Token::Type::kMat4x3, source, "mat4x3"};
  if (str == "mat4x4")
    return {Token::Type::kMat4x4, source, "mat4x4"};
  if (str == "private")
    return {Token::Type::kPrivate, source, "private"};
  if (str == "ptr")
    return {Token::Type::kPtr, source, "ptr"};
  if (str == "return")
    return {Token::Type::kReturn, source, "return"};
  if (str == "sampler")
    return {Token::Type::kSampler, source, "sampler"};
  if (str == "sampler_comparison")
    return {Token::Type::kComparisonSampler, source, "sampler_comparison"};
  if (str == "storage_buffer" || str == "storage")
    return {Token::Type::kStorage, source, "storage"};
  if (str == "struct")
    return {Token::Type::kStruct, source, "struct"};
  if (str == "switch")
    return {Token::Type::kSwitch, source, "switch"};
  if (str == "texture_1d")
    return {Token::Type::kTextureSampled1d, source, "texture_1d"};
  if (str == "texture_2d")
    return {Token::Type::kTextureSampled2d, source, "texture_2d"};
  if (str == "texture_2d_array")
    return {Token::Type::kTextureSampled2dArray, source, "texture_2d_array"};
  if (str == "texture_3d")
    return {Token::Type::kTextureSampled3d, source, "texture_3d"};
  if (str == "texture_cube")
    return {Token::Type::kTextureSampledCube, source, "texture_cube"};
  if (str == "texture_cube_array") {
    return {Token::Type::kTextureSampledCubeArray, source,
            "texture_cube_array"};
  }
  if (str == "texture_depth_2d")
    return {Token::Type::kTextureDepth2d, source, "texture_depth_2d"};
  if (str == "texture_depth_2d_array") {
    return {Token::Type::kTextureDepth2dArray, source,
            "texture_depth_2d_array"};
  }
  if (str == "texture_depth_cube")
    return {Token::Type::kTextureDepthCube, source, "texture_depth_cube"};
  if (str == "texture_depth_cube_array") {
    return {Token::Type::kTextureDepthCubeArray, source,
            "texture_depth_cube_array"};
  }
  if (str == "texture_depth_multisampled_2d") {
    return {Token::Type::kTextureDepthMultisampled2d, source,
            "texture_depth_multisampled_2d"};
  }
  if (str == "texture_external") {
    return {Token::Type::kTextureExternal, source, "texture_external"};
  }
  if (str == "texture_multisampled_2d") {
    return {Token::Type::kTextureMultisampled2d, source,
            "texture_multisampled_2d"};
  }
  if (str == "texture_storage_1d") {
    return {Token::Type::kTextureStorage1d, source, "texture_storage_1d"};
  }
  if (str == "texture_storage_2d") {
    return {Token::Type::kTextureStorage2d, source, "texture_storage_2d"};
  }
  if (str == "texture_storage_2d_array") {
    return {Token::Type::kTextureStorage2dArray, source,
            "texture_storage_2d_array"};
  }
  if (str == "texture_storage_3d") {
    return {Token::Type::kTextureStorage3d, source, "texture_storage_3d"};
  }
  if (str == "true")
    return {Token::Type::kTrue, source, "true"};
  if (str == "type")
    return {Token::Type::kType, source, "type"};
  if (str == "u32")
    return {Token::Type::kU32, source, "u32"};
  if (str == "uniform")
    return {Token::Type::kUniform, source, "uniform"};
  if (str == "var")
    return {Token::Type::kVar, source, "var"};
  if (str == "vec2")
    return {Token::Type::kVec2, source, "vec2"};
  if (str == "vec3")
    return {Token::Type::kVec3, source, "vec3"};
  if (str == "vec4")
    return {Token::Type::kVec4, source, "vec4"};
  if (str == "workgroup")
    return {Token::Type::kWorkgroup, source, "workgroup"};
  return {};
}

}  // namespace wgsl
}  // namespace reader
}  // namespace tint
