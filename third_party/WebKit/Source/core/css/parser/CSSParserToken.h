// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSParserToken_h
#define CSSParserToken_h

#include "core/CoreExport.h"
#include "core/css/CSSPrimitiveValue.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/text/StringView.h"

namespace blink {

enum CSSParserTokenType {
  kIdentToken = 0,
  kFunctionToken,
  kAtKeywordToken,
  kHashToken,
  kUrlToken,
  kBadUrlToken,
  kDelimiterToken,
  kNumberToken,
  kPercentageToken,
  kDimensionToken,
  kIncludeMatchToken,
  kDashMatchToken,
  kPrefixMatchToken,
  kSuffixMatchToken,
  kSubstringMatchToken,
  kColumnToken,
  kUnicodeRangeToken,
  kWhitespaceToken,
  kCDOToken,
  kCDCToken,
  kColonToken,
  kSemicolonToken,
  kCommaToken,
  kLeftParenthesisToken,
  kRightParenthesisToken,
  kLeftBracketToken,
  kRightBracketToken,
  kLeftBraceToken,
  kRightBraceToken,
  kStringToken,
  kBadStringToken,
  kEOFToken,
  kCommentToken,
};

enum NumericSign {
  kNoSign,
  kPlusSign,
  kMinusSign,
};

enum NumericValueType {
  kIntegerValueType,
  kNumberValueType,
};

enum HashTokenType {
  kHashTokenId,
  kHashTokenUnrestricted,
};

class CORE_EXPORT CSSParserToken {
  USING_FAST_MALLOC(CSSParserToken);

 public:
  enum BlockType {
    kNotBlock,
    kBlockStart,
    kBlockEnd,
  };

  CSSParserToken(CSSParserTokenType type, BlockType block_type = kNotBlock)
      : type_(type), block_type_(block_type) {}
  CSSParserToken(CSSParserTokenType type,
                 StringView value,
                 BlockType block_type = kNotBlock)
      : type_(type), block_type_(block_type) {
    InitValueFromStringView(value);
    id_ = -1;
  }

  CSSParserToken(CSSParserTokenType, UChar);  // for DelimiterToken
  CSSParserToken(CSSParserTokenType,
                 double,
                 NumericValueType,
                 NumericSign);  // for NumberToken
  CSSParserToken(CSSParserTokenType,
                 UChar32,
                 UChar32);  // for UnicodeRangeToken

  CSSParserToken(HashTokenType, StringView);

  bool operator==(const CSSParserToken& other) const;
  bool operator!=(const CSSParserToken& other) const {
    return !(*this == other);
  }

  // Converts NumberToken to DimensionToken.
  void ConvertToDimensionWithUnit(StringView);

  // Converts NumberToken to PercentageToken.
  void ConvertToPercentage();

  CSSParserTokenType GetType() const {
    return static_cast<CSSParserTokenType>(type_);
  }
  StringView Value() const {
    if (value_is8_bit_)
      return StringView(reinterpret_cast<const LChar*>(value_data_char_raw_),
                        value_length_);
    return StringView(reinterpret_cast<const UChar*>(value_data_char_raw_),
                      value_length_);
  }

  UChar Delimiter() const;
  NumericSign GetNumericSign() const;
  NumericValueType GetNumericValueType() const;
  double NumericValue() const;
  HashTokenType GetHashTokenType() const {
    DCHECK_EQ(type_, static_cast<unsigned>(kHashToken));
    return hash_token_type_;
  }
  BlockType GetBlockType() const { return static_cast<BlockType>(block_type_); }
  CSSPrimitiveValue::UnitType GetUnitType() const {
    return static_cast<CSSPrimitiveValue::UnitType>(unit_);
  }
  UChar32 UnicodeRangeStart() const {
    DCHECK_EQ(type_, static_cast<unsigned>(kUnicodeRangeToken));
    return unicode_range_.start;
  }
  UChar32 UnicodeRangeEnd() const {
    DCHECK_EQ(type_, static_cast<unsigned>(kUnicodeRangeToken));
    return unicode_range_.end;
  }
  CSSValueID Id() const;
  CSSValueID FunctionId() const;

  bool HasStringBacking() const;

  CSSPropertyID ParseAsUnresolvedCSSPropertyID() const;

  void Serialize(StringBuilder&) const;

  CSSParserToken CopyWithUpdatedString(const StringView&) const;

 private:
  void InitValueFromStringView(StringView string) {
    value_length_ = string.length();
    value_is8_bit_ = string.Is8Bit();
    value_data_char_raw_ = string.Bytes();
  }
  unsigned type_ : 6;                // CSSParserTokenType
  unsigned block_type_ : 2;          // BlockType
  unsigned numeric_value_type_ : 1;  // NumericValueType
  unsigned numeric_sign_ : 2;        // NumericSign
  unsigned unit_ : 7;                // CSSPrimitiveValue::UnitType

  bool ValueDataCharRawEqual(const CSSParserToken& other) const;

  // value_... is an unpacked StringView so that we can pack it
  // tightly with the rest of this object for a smaller object size.
  bool value_is8_bit_ : 1;
  unsigned value_length_;
  const void* value_data_char_raw_;  // Either LChar* or UChar*.

  union {
    UChar delimiter_;
    HashTokenType hash_token_type_;
    double numeric_value_;
    mutable int id_;

    struct {
      UChar32 start;
      UChar32 end;
    } unicode_range_;
  };
};

}  // namespace blink

#endif  // CSSSParserToken_h
