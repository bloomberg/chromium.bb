// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSTokenizer_h
#define CSSTokenizer_h

#include "core/CoreExport.h"
#include "core/css/parser/CSSParserToken.h"
#include "core/css/parser/CSSTokenizerInputStream.h"
#include "core/html/parser/InputStreamPreprocessor.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/text/StringView.h"
#include "platform/wtf/text/WTFString.h"

#include <climits>

namespace blink {

class CSSTokenizerInputStream;
class CSSParserObserverWrapper;
class CSSParserTokenRange;

class CORE_EXPORT CSSTokenizer {
  WTF_MAKE_NONCOPYABLE(CSSTokenizer);
  DISALLOW_NEW();

 public:
  CSSTokenizer(const String&);
  CSSTokenizer(const String&, CSSParserObserverWrapper&);  // For the inspector

  CSSParserTokenRange TokenRange();
  unsigned TokenCount();

  Vector<String> TakeEscapedStrings() { return std::move(string_pool_); }

 private:
  CSSParserToken TokenizeSingle();
  void EnsureTokenizedToEOF();
  unsigned CurrentSize() const;

  CSSParserToken NextToken();

  UChar Consume();
  void Reconsume(UChar);

  CSSParserToken ConsumeNumericToken();
  CSSParserToken ConsumeIdentLikeToken();
  CSSParserToken ConsumeNumber();
  CSSParserToken ConsumeStringTokenUntil(UChar);
  CSSParserToken ConsumeUnicodeRange();
  CSSParserToken ConsumeUrlToken();

  void ConsumeBadUrlRemnants();
  void ConsumeSingleWhitespaceIfNext();
  void ConsumeUntilCommentEndFound();

  bool ConsumeIfNext(UChar);
  StringView ConsumeName();
  UChar32 ConsumeEscape();

  bool NextTwoCharsAreValidEscape();
  bool NextCharsAreNumber(UChar);
  bool NextCharsAreNumber();
  bool NextCharsAreIdentifier(UChar);
  bool NextCharsAreIdentifier();

  CSSParserToken BlockStart(CSSParserTokenType);
  CSSParserToken BlockStart(CSSParserTokenType block_type,
                            CSSParserTokenType,
                            StringView);
  CSSParserToken BlockEnd(CSSParserTokenType, CSSParserTokenType start_type);

  CSSParserToken WhiteSpace(UChar);
  CSSParserToken LeftParenthesis(UChar);
  CSSParserToken RightParenthesis(UChar);
  CSSParserToken LeftBracket(UChar);
  CSSParserToken RightBracket(UChar);
  CSSParserToken LeftBrace(UChar);
  CSSParserToken RightBrace(UChar);
  CSSParserToken PlusOrFullStop(UChar);
  CSSParserToken Comma(UChar);
  CSSParserToken HyphenMinus(UChar);
  CSSParserToken Asterisk(UChar);
  CSSParserToken LessThan(UChar);
  CSSParserToken Solidus(UChar);
  CSSParserToken Colon(UChar);
  CSSParserToken SemiColon(UChar);
  CSSParserToken Hash(UChar);
  CSSParserToken CircumflexAccent(UChar);
  CSSParserToken DollarSign(UChar);
  CSSParserToken VerticalLine(UChar);
  CSSParserToken Tilde(UChar);
  CSSParserToken CommercialAt(UChar);
  CSSParserToken ReverseSolidus(UChar);
  CSSParserToken AsciiDigit(UChar);
  CSSParserToken LetterU(UChar);
  CSSParserToken NameStart(UChar);
  CSSParserToken StringStart(UChar);
  CSSParserToken EndOfFile(UChar);

  StringView RegisterString(const String&);

  using CodePoint = CSSParserToken (CSSTokenizer::*)(UChar);
  static const CodePoint kCodePoints[];

  CSSTokenizerInputStream input_;
  Vector<CSSParserTokenType, 8> block_stack_;

  Vector<CSSParserToken> tokens_;
  // We only allocate strings when escapes are used.
  Vector<String> string_pool_;

  friend class CSSParserTokenStream;
};

}  // namespace blink

#endif  // CSSTokenizer_h
