// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSProtoConverter_h
#define CSSProtoConverter_h

#include <string>

#include "third_party/WebKit/Source/core/css/parser/CSS.pb.h"

namespace css_proto_converter {

class Converter {
 public:
  Converter();
  std::string Convert(const StyleSheet&);

 private:
  std::string string_;
  static const std::string kPseudoLookupTable[];
  static const std::string kMediaTypeLookupTable[];
  static const std::string kMfNameLookupTable[];
  static const std::string kImportLookupTable[];
  static const std::string kEncodingLookupTable[];
  static const std::string kValueLookupTable[];
  static const std::string kPropertyLookupTable[];

  void Visit(const Unicode&);
  void Visit(const Escape&);
  void Visit(const Nmstart&);
  void Visit(const Nmchar&);
  void Visit(const String&);
  void Visit(const StringCharOrQuote&, bool use_single);
  void Visit(const StringChar&);
  void Visit(const Ident&);
  void Visit(const Num&);
  void Visit(const UrlChar&);
  void Visit(const W&);
  void Visit(const UnrepeatedW&);
  void Visit(const Nl&);
  void Visit(const Length&);
  void Visit(const Angle&);
  void Visit(const Time&);
  void Visit(const Freq&);
  void Visit(const Uri&);
  void Visit(const FunctionToken&);
  void Visit(const StyleSheet&);
  void Visit(const CharsetDeclaration&);
  void Visit(const RulesetOrMediaOrPageOrFontFace&);
  void Visit(const Import&);
  void Visit(const Namespace&);
  void Visit(const NamespacePrefix&);
  void Visit(const Media&);
  void Visit(const Page&);
  void Visit(const DeclarationList&);
  void Visit(const FontFace&);
  void Visit(const Operator&);
  void Visit(const UnaryOperator&);
  void Visit(const Property&);
  void Visit(const Ruleset&);
  void Visit(const SelectorList&);
  void Visit(const Declaration&);
  void Visit(const NonEmptyDeclaration&);
  void Visit(const Expr&, int declaration_value_id = 0);
  void Visit(const OperatorTerm&);
  void Visit(const Term&);
  void Visit(const TermPart&);
  void Visit(const Function&);
  void Visit(const Hexcolor&);
  void Visit(const HexcolorThree&);
  void Visit(const MediaQueryList&);
  void Visit(const MediaQuery&);
  void Visit(const MediaQueryPartTwo&);
  void Visit(const MediaType&);
  void Visit(const MediaNot&);
  void Visit(const MediaAnd&);
  void Visit(const MediaOr&);
  void Visit(const MediaInParens&);
  void Visit(const MediaFeature&);
  void Visit(const MfPlain&);
  void Visit(const MfBool&);
  void Visit(const MfName&);
  void Visit(const MfValue&);
  void Visit(const MediaCondition&);
  void Visit(const MediaConditionWithoutOr&);
  void Visit(const Selector&, bool is_first);
  void Visit(const PseudoPage&);
  void Reset();
  template <size_t TableSize>
  void AppendTableValue(int id, const std::string (&lookup_table)[TableSize]);
};
};  // namespace css_proto_converter

#endif  // CSSProtoConverter_h
