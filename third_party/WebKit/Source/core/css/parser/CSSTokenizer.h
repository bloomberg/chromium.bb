// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSTokenizer_h
#define CSSTokenizer_h

#include "base/macros.h"
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

class CORE_EXPORT CSSTokenizer {
  DISALLOW_NEW();

 public:
  CSSTokenizer(const String&, size_t offset = 0);

  Vector<CSSParserToken, 32> TokenizeToEOF();
  unsigned TokenCount();

  size_t Offset() const { return input_.Offset(); }
  size_t PreviousOffset() const { return prev_offset_; }

 private:
  CSSParserToken TokenizeSingle();
  CSSParserToken TokenizeSingleWithComments();

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

  // We only allocate strings when escapes are used.
  Vector<String> string_pool_;

  friend class CSSParserTokenStream;

  size_t prev_offset_ = 0;
  size_t token_count_ = 0;
  DISALLOW_COPY_AND_ASSIGN(CSSTokenizer);
};

}  // namespace blink

#endif  // CSSTokenizer_h
