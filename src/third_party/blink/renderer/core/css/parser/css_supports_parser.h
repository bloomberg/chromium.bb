// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_SUPPORTS_PARSER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_SUPPORTS_PARSER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class CSSParserImpl;
class CSSParserToken;
class CSSParserTokenStream;

class CORE_EXPORT CSSSupportsParser {
  STACK_ALLOCATED();

 public:
  enum class Result {
    // kUnsupported/kSupported means that we parsed the @supports
    // successfully, and conclusively determined that we either support or
    // don't support the feature.
    kUnsupported,
    kSupported,
    // kUnknown is a special value used for productions that only match
    // <general-enclosed> [1]. See note regarding Kleene 3-valued logic [2]
    // for explanation of how this is different from kUnsupported.
    //
    // [1] https://drafts.csswg.org/css-conditional-3/#at-supports
    // [2] https://drafts.csswg.org/mediaqueries-4/#evaluating
    kUnknown,
    // This is used to signal parse failure in the @supports syntax itself.
    // This means that for a production like:
    //
    // <supports-in-parens> = ( <supports-condition> )
    //                    | <supports-feature>
    //                    | <general-enclosed>
    //
    // If ConsumeSupportsCondition returns a kParseFailure, we'll proceed to
    // trying the ConsumeGeneralEnclosed branch. Had however
    // ConsumeSupportsCondition returned kUnsupported, we would consider this a
    // conclusive answer, and would have returned kUnsupported without trying
    // any further parsing branches.
    kParseFailure
  };

  static Result ConsumeSupportsCondition(CSSParserTokenStream&, CSSParserImpl&);

 private:
  friend class CSSSupportsParserTest;

  CSSSupportsParser(CSSParserImpl& parser) : parser_(parser) {}

  // True if the given token is a kIdentToken with the specified value
  // (case-insensitive).
  static bool AtIdent(const CSSParserToken&, const char*);

  // If the current token is a kIdentToken with the specified value (case
  // insensitive), consumes the token and returns true.
  static bool ConsumeIfIdent(CSSParserTokenStream&, const char*);

  // Parsing functions follow, as defined by:
  // https://drafts.csswg.org/css-conditional-3/#typedef-supports-condition

  // <supports-condition> = not <supports-in-parens>
  //                   | <supports-in-parens> [ and <supports-in-parens> ]*
  //                   | <supports-in-parens> [ or <supports-in-parens> ]*
  Result ConsumeSupportsCondition(CSSParserTokenStream&);

  // <supports-in-parens> = ( <supports-condition> )
  //                    | <supports-feature>
  //                    | <general-enclosed>
  Result ConsumeSupportsInParens(CSSParserTokenStream&);

  // <supports-feature> = <supports-selector-fn> | <supports-decl>
  Result ConsumeSupportsFeature(const CSSParserToken&, CSSParserTokenStream&);

  // <supports-selector-fn> = selector( <complex-selector> )
  Result ConsumeSupportsSelectorFn(const CSSParserToken&,
                                   CSSParserTokenStream&);

  // <supports-decl> = ( <declaration> )
  Result ConsumeSupportsDecl(const CSSParserToken&, CSSParserTokenStream&);

  // <general-enclosed>
  Result ConsumeGeneralEnclosed(const CSSParserToken&, CSSParserTokenStream&);

  // Parsing helpers.
  static bool IsSupportsInParens(const CSSParserToken&);
  static bool IsEnclosedSupportsCondition(const CSSParserToken&,
                                          const CSSParserToken&);
  static bool IsSupportsSelectorFn(const CSSParserToken&,
                                   const CSSParserToken&);
  static bool IsSupportsDecl(const CSSParserToken&, const CSSParserToken&);
  static bool IsSupportsFeature(const CSSParserToken&, const CSSParserToken&);
  static bool IsGeneralEnclosed(const CSSParserToken&);

  CSSParserImpl& parser_;
};

inline CSSSupportsParser::Result operator!(CSSSupportsParser::Result result) {
  using Result = CSSSupportsParser::Result;
  if (result == Result::kUnsupported)
    return Result::kSupported;
  if (result == Result::kSupported)
    return Result::kUnsupported;
  return result;
}

inline CSSSupportsParser::Result operator&(CSSSupportsParser::Result a,
                                           CSSSupportsParser::Result b) {
  using Result = CSSSupportsParser::Result;
  if (a == Result::kParseFailure || b == Result::kParseFailure)
    return Result::kParseFailure;
  if (a == Result::kUnknown && b == Result::kUnknown)
    return Result::kUnknown;
  if (a != Result::kSupported || b != Result::kSupported)
    return Result::kUnsupported;
  return Result::kSupported;
}

inline CSSSupportsParser::Result operator|(CSSSupportsParser::Result a,
                                           CSSSupportsParser::Result b) {
  using Result = CSSSupportsParser::Result;
  if (a == Result::kParseFailure || b == Result::kParseFailure)
    return Result::kParseFailure;
  if (a == Result::kSupported || b == Result::kSupported)
    return Result::kSupported;
  if (a == Result::kUnknown || b == Result::kUnknown)
    return Result::kUnknown;
  return Result::kUnsupported;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_SUPPORTS_PARSER_H_
