// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/CSSVariableData.h"

#include "core/css/CSSSyntaxDescriptor.h"
#include "core/css/parser/CSSParser.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include "platform/wtf/text/StringBuilder.h"
#include "platform/wtf/text/StringView.h"

namespace blink {

StylePropertySet* CSSVariableData::PropertySet() {
  DCHECK(!needs_variable_resolution_);
  if (!cached_property_set_) {
    property_set_ = CSSParser::ParseCustomPropertySet(tokens_);
    cached_property_set_ = true;
  }
  return property_set_.Get();
}

template <typename CharacterType>
void CSSVariableData::UpdateTokens(const CSSParserTokenRange& range) {
  const CharacterType* current_offset =
      backing_string_.GetCharacters<CharacterType>();
  for (const CSSParserToken& token : range) {
    if (token.HasStringBacking()) {
      unsigned length = token.Value().length();
      StringView string(current_offset, length);
      tokens_.push_back(token.CopyWithUpdatedString(string));
      current_offset += length;
    } else {
      tokens_.push_back(token);
    }
  }
  DCHECK(current_offset == backing_string_.GetCharacters<CharacterType>() +
                               backing_string_.length());
}

bool CSSVariableData::operator==(const CSSVariableData& other) const {
  return Tokens() == other.Tokens();
}

void CSSVariableData::ConsumeAndUpdateTokens(const CSSParserTokenRange& range) {
  StringBuilder string_builder;
  CSSParserTokenRange local_range = range;

  while (!local_range.AtEnd()) {
    CSSParserToken token = local_range.Consume();
    if (token.HasStringBacking())
      string_builder.Append(token.Value());
  }
  backing_string_ = string_builder.ToString();
  if (backing_string_.Is8Bit())
    UpdateTokens<LChar>(range);
  else
    UpdateTokens<UChar>(range);
}

CSSVariableData::CSSVariableData(const CSSParserTokenRange& range,
                                 bool is_animation_tainted,
                                 bool needs_variable_resolution)
    : is_animation_tainted_(is_animation_tainted),
      needs_variable_resolution_(needs_variable_resolution),
      cached_property_set_(false) {
  DCHECK(!range.AtEnd());
  ConsumeAndUpdateTokens(range);
}

const CSSValue* CSSVariableData::ParseForSyntax(
    const CSSSyntaxDescriptor& syntax) const {
  DCHECK(!NeedsVariableResolution());
  // TODO(timloh): This probably needs a proper parser context for
  // relative URL resolution.
  return syntax.Parse(TokenRange(), StrictCSSParserContext(),
                      is_animation_tainted_);
}

}  // namespace blink
