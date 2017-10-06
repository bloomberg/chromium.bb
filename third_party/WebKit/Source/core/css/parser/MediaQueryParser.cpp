// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/parser/MediaQueryParser.h"

#include "core/css/parser/CSSTokenizer.h"
#include "core/media_type_names.h"
#include "platform/wtf/Vector.h"

namespace blink {

RefPtr<MediaQuerySet> MediaQueryParser::ParseMediaQuerySet(
    const String& query_string) {
  return ParseMediaQuerySet(
      CSSParserTokenRange(CSSTokenizer(query_string).TokenizeToEOF()));
}

RefPtr<MediaQuerySet> MediaQueryParser::ParseMediaQuerySet(
    CSSParserTokenRange range) {
  return MediaQueryParser(kMediaQuerySetParser).ParseImpl(range);
}

RefPtr<MediaQuerySet> MediaQueryParser::ParseMediaCondition(
    CSSParserTokenRange range) {
  return MediaQueryParser(kMediaConditionParser).ParseImpl(range);
}

const MediaQueryParser::State MediaQueryParser::kReadRestrictor =
    &MediaQueryParser::ReadRestrictor;
const MediaQueryParser::State MediaQueryParser::kReadMediaNot =
    &MediaQueryParser::ReadMediaNot;
const MediaQueryParser::State MediaQueryParser::kReadMediaType =
    &MediaQueryParser::ReadMediaType;
const MediaQueryParser::State MediaQueryParser::kReadAnd =
    &MediaQueryParser::ReadAnd;
const MediaQueryParser::State MediaQueryParser::kReadFeatureStart =
    &MediaQueryParser::ReadFeatureStart;
const MediaQueryParser::State MediaQueryParser::kReadFeature =
    &MediaQueryParser::ReadFeature;
const MediaQueryParser::State MediaQueryParser::kReadFeatureColon =
    &MediaQueryParser::ReadFeatureColon;
const MediaQueryParser::State MediaQueryParser::kReadFeatureValue =
    &MediaQueryParser::ReadFeatureValue;
const MediaQueryParser::State MediaQueryParser::kReadFeatureEnd =
    &MediaQueryParser::ReadFeatureEnd;
const MediaQueryParser::State MediaQueryParser::kSkipUntilComma =
    &MediaQueryParser::SkipUntilComma;
const MediaQueryParser::State MediaQueryParser::kSkipUntilBlockEnd =
    &MediaQueryParser::SkipUntilBlockEnd;
const MediaQueryParser::State MediaQueryParser::kDone = &MediaQueryParser::Done;

MediaQueryParser::MediaQueryParser(ParserType parser_type)
    : parser_type_(parser_type), query_set_(MediaQuerySet::Create()) {
  if (parser_type == kMediaQuerySetParser)
    state_ = &MediaQueryParser::ReadRestrictor;
  else  // MediaConditionParser
    state_ = &MediaQueryParser::ReadMediaNot;
}

MediaQueryParser::~MediaQueryParser() {}

void MediaQueryParser::SetStateAndRestrict(
    State state,
    MediaQuery::RestrictorType restrictor) {
  media_query_data_.SetRestrictor(restrictor);
  state_ = state;
}

// State machine member functions start here
void MediaQueryParser::ReadRestrictor(CSSParserTokenType type,
                                      const CSSParserToken& token) {
  ReadMediaType(type, token);
}

void MediaQueryParser::ReadMediaNot(CSSParserTokenType type,
                                    const CSSParserToken& token) {
  if (type == kIdentToken && EqualIgnoringASCIICase(token.Value(), "not"))
    SetStateAndRestrict(kReadFeatureStart, MediaQuery::kNot);
  else
    ReadFeatureStart(type, token);
}

static bool IsRestrictorOrLogicalOperator(const CSSParserToken& token) {
  // FIXME: it would be more efficient to use lower-case always for tokenValue.
  return EqualIgnoringASCIICase(token.Value(), "not") ||
         EqualIgnoringASCIICase(token.Value(), "and") ||
         EqualIgnoringASCIICase(token.Value(), "or") ||
         EqualIgnoringASCIICase(token.Value(), "only");
}

void MediaQueryParser::ReadMediaType(CSSParserTokenType type,
                                     const CSSParserToken& token) {
  if (type == kLeftParenthesisToken) {
    if (media_query_data_.Restrictor() != MediaQuery::kNone)
      state_ = kSkipUntilComma;
    else
      state_ = kReadFeature;
  } else if (type == kIdentToken) {
    if (state_ == kReadRestrictor &&
        EqualIgnoringASCIICase(token.Value(), "not")) {
      SetStateAndRestrict(kReadMediaType, MediaQuery::kNot);
    } else if (state_ == kReadRestrictor &&
               EqualIgnoringASCIICase(token.Value(), "only")) {
      SetStateAndRestrict(kReadMediaType, MediaQuery::kOnly);
    } else if (media_query_data_.Restrictor() != MediaQuery::kNone &&
               IsRestrictorOrLogicalOperator(token)) {
      state_ = kSkipUntilComma;
    } else {
      media_query_data_.SetMediaType(token.Value().ToString());
      state_ = kReadAnd;
    }
  } else if (type == kEOFToken &&
             (!query_set_->QueryVector().size() || state_ != kReadRestrictor)) {
    state_ = kDone;
  } else {
    state_ = kSkipUntilComma;
    if (type == kCommaToken)
      SkipUntilComma(type, token);
  }
}

void MediaQueryParser::ReadAnd(CSSParserTokenType type,
                               const CSSParserToken& token) {
  if (type == kIdentToken && EqualIgnoringASCIICase(token.Value(), "and")) {
    state_ = kReadFeatureStart;
  } else if (type == kCommaToken && parser_type_ != kMediaConditionParser) {
    query_set_->AddMediaQuery(media_query_data_.TakeMediaQuery());
    state_ = kReadRestrictor;
  } else if (type == kEOFToken) {
    state_ = kDone;
  } else {
    state_ = kSkipUntilComma;
  }
}

void MediaQueryParser::ReadFeatureStart(CSSParserTokenType type,
                                        const CSSParserToken& token) {
  if (type == kLeftParenthesisToken)
    state_ = kReadFeature;
  else
    state_ = kSkipUntilComma;
}

void MediaQueryParser::ReadFeature(CSSParserTokenType type,
                                   const CSSParserToken& token) {
  if (type == kIdentToken) {
    media_query_data_.SetMediaFeature(token.Value().ToString());
    state_ = kReadFeatureColon;
  } else {
    state_ = kSkipUntilComma;
  }
}

void MediaQueryParser::ReadFeatureColon(CSSParserTokenType type,
                                        const CSSParserToken& token) {
  if (type == kColonToken)
    state_ = kReadFeatureValue;
  else if (type == kRightParenthesisToken || type == kEOFToken)
    ReadFeatureEnd(type, token);
  else
    state_ = kSkipUntilBlockEnd;
}

void MediaQueryParser::ReadFeatureValue(CSSParserTokenType type,
                                        const CSSParserToken& token) {
  if (type == kDimensionToken &&
      token.GetUnitType() == CSSPrimitiveValue::UnitType::kUnknown) {
    state_ = kSkipUntilComma;
  } else {
    if (media_query_data_.TryAddParserToken(type, token))
      state_ = kReadFeatureEnd;
    else
      state_ = kSkipUntilBlockEnd;
  }
}

void MediaQueryParser::ReadFeatureEnd(CSSParserTokenType type,
                                      const CSSParserToken& token) {
  if (type == kRightParenthesisToken || type == kEOFToken) {
    if (media_query_data_.AddExpression())
      state_ = kReadAnd;
    else
      state_ = kSkipUntilComma;
  } else if (type == kDelimiterToken && token.Delimiter() == '/') {
    media_query_data_.TryAddParserToken(type, token);
    state_ = kReadFeatureValue;
  } else {
    state_ = kSkipUntilBlockEnd;
  }
}

void MediaQueryParser::SkipUntilComma(CSSParserTokenType type,
                                      const CSSParserToken& token) {
  if ((type == kCommaToken && !block_watcher_.BlockLevel()) ||
      type == kEOFToken) {
    state_ = kReadRestrictor;
    media_query_data_.Clear();
    query_set_->AddMediaQuery(MediaQuery::CreateNotAll());
  }
}

void MediaQueryParser::SkipUntilBlockEnd(CSSParserTokenType type,
                                         const CSSParserToken& token) {
  if (token.GetBlockType() == CSSParserToken::kBlockEnd &&
      !block_watcher_.BlockLevel())
    state_ = kSkipUntilComma;
}

void MediaQueryParser::Done(CSSParserTokenType type,
                            const CSSParserToken& token) {}

void MediaQueryParser::HandleBlocks(const CSSParserToken& token) {
  if (token.GetBlockType() == CSSParserToken::kBlockStart &&
      (token.GetType() != kLeftParenthesisToken || block_watcher_.BlockLevel()))
    state_ = kSkipUntilBlockEnd;
}

void MediaQueryParser::ProcessToken(const CSSParserToken& token) {
  CSSParserTokenType type = token.GetType();

  HandleBlocks(token);
  block_watcher_.HandleToken(token);

  // Call the function that handles current state
  if (type != kWhitespaceToken)
    ((this)->*(state_))(type, token);
}

// The state machine loop
RefPtr<MediaQuerySet> MediaQueryParser::ParseImpl(CSSParserTokenRange range) {
  while (!range.AtEnd())
    ProcessToken(range.Consume());

  // FIXME: Can we get rid of this special case?
  if (parser_type_ == kMediaQuerySetParser)
    ProcessToken(CSSParserToken(kEOFToken));

  if (state_ != kReadAnd && state_ != kReadRestrictor && state_ != kDone &&
      state_ != kReadMediaNot)
    query_set_->AddMediaQuery(MediaQuery::CreateNotAll());
  else if (media_query_data_.CurrentMediaQueryChanged())
    query_set_->AddMediaQuery(media_query_data_.TakeMediaQuery());

  return query_set_;
}

MediaQueryData::MediaQueryData()
    : restrictor_(MediaQuery::kNone),
      media_type_(MediaTypeNames::all),
      media_type_set_(false) {}

void MediaQueryData::Clear() {
  restrictor_ = MediaQuery::kNone;
  media_type_ = MediaTypeNames::all;
  media_type_set_ = false;
  media_feature_ = String();
  value_list_.clear();
  expressions_.clear();
}

std::unique_ptr<MediaQuery> MediaQueryData::TakeMediaQuery() {
  std::unique_ptr<MediaQuery> media_query = MediaQuery::Create(
      restrictor_, std::move(media_type_), std::move(expressions_));
  Clear();
  return media_query;
}

bool MediaQueryData::AddExpression() {
  MediaQueryExp expression = MediaQueryExp::Create(media_feature_, value_list_);
  expressions_.push_back(expression);
  value_list_.clear();
  return expression.IsValid();
}

bool MediaQueryData::TryAddParserToken(CSSParserTokenType type,
                                       const CSSParserToken& token) {
  if (type == kNumberToken || type == kPercentageToken ||
      type == kDimensionToken || type == kDelimiterToken ||
      type == kIdentToken) {
    value_list_.push_back(token);
    return true;
  }

  return false;
}

void MediaQueryData::SetMediaType(const String& media_type) {
  media_type_ = media_type;
  media_type_set_ = true;
}

}  // namespace blink
