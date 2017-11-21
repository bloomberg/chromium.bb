// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaQueryParser_h
#define MediaQueryParser_h

#include "base/macros.h"
#include "core/CoreExport.h"
#include "core/css/MediaList.h"
#include "core/css/MediaQuery.h"
#include "core/css/MediaQueryExp.h"
#include "core/css/parser/CSSParserToken.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include "core/css/parser/MediaQueryBlockWatcher.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class MediaQuerySet;

class MediaQueryData {
  STACK_ALLOCATED();

 private:
  MediaQuery::RestrictorType restrictor_;
  String media_type_;
  ExpressionHeapVector expressions_;
  String media_feature_;
  Vector<CSSParserToken, 4> value_list_;
  bool media_type_set_;

 public:
  MediaQueryData();
  void Clear();
  bool AddExpression();
  bool TryAddParserToken(CSSParserTokenType, const CSSParserToken&);
  void SetMediaType(const String&);
  std::unique_ptr<MediaQuery> TakeMediaQuery();

  inline bool CurrentMediaQueryChanged() const {
    return (restrictor_ != MediaQuery::kNone || media_type_set_ ||
            expressions_.size() > 0);
  }
  inline MediaQuery::RestrictorType Restrictor() { return restrictor_; }

  inline void SetRestrictor(MediaQuery::RestrictorType restrictor) {
    restrictor_ = restrictor;
  }

  inline void SetMediaFeature(const String& str) { media_feature_ = str; }
  DISALLOW_COPY_AND_ASSIGN(MediaQueryData);
};

class CORE_EXPORT MediaQueryParser {
  STACK_ALLOCATED();

 public:
  static scoped_refptr<MediaQuerySet> ParseMediaQuerySet(const String&);
  static scoped_refptr<MediaQuerySet> ParseMediaQuerySet(CSSParserTokenRange);
  static scoped_refptr<MediaQuerySet> ParseMediaCondition(CSSParserTokenRange);

 private:
  enum ParserType {
    kMediaQuerySetParser,
    kMediaConditionParser,
  };

  MediaQueryParser(ParserType);
  virtual ~MediaQueryParser();

  scoped_refptr<MediaQuerySet> ParseImpl(CSSParserTokenRange);

  void ProcessToken(const CSSParserToken&);

  void ReadRestrictor(CSSParserTokenType, const CSSParserToken&);
  void ReadMediaNot(CSSParserTokenType, const CSSParserToken&);
  void ReadMediaType(CSSParserTokenType, const CSSParserToken&);
  void ReadAnd(CSSParserTokenType, const CSSParserToken&);
  void ReadFeatureStart(CSSParserTokenType, const CSSParserToken&);
  void ReadFeature(CSSParserTokenType, const CSSParserToken&);
  void ReadFeatureColon(CSSParserTokenType, const CSSParserToken&);
  void ReadFeatureValue(CSSParserTokenType, const CSSParserToken&);
  void ReadFeatureEnd(CSSParserTokenType, const CSSParserToken&);
  void SkipUntilComma(CSSParserTokenType, const CSSParserToken&);
  void SkipUntilBlockEnd(CSSParserTokenType, const CSSParserToken&);
  void Done(CSSParserTokenType, const CSSParserToken&);

  using State = void (MediaQueryParser::*)(CSSParserTokenType,
                                           const CSSParserToken&);

  void SetStateAndRestrict(State, MediaQuery::RestrictorType);
  void HandleBlocks(const CSSParserToken&);

  State state_;
  ParserType parser_type_;
  MediaQueryData media_query_data_;
  scoped_refptr<MediaQuerySet> query_set_;
  MediaQueryBlockWatcher block_watcher_;

  const static State kReadRestrictor;
  const static State kReadMediaNot;
  const static State kReadMediaType;
  const static State kReadAnd;
  const static State kReadFeatureStart;
  const static State kReadFeature;
  const static State kReadFeatureColon;
  const static State kReadFeatureValue;
  const static State kReadFeatureEnd;
  const static State kSkipUntilComma;
  const static State kSkipUntilBlockEnd;
  const static State kDone;
  DISALLOW_COPY_AND_ASSIGN(MediaQueryParser);
};

}  // namespace blink

#endif  // MediaQueryParser_h
